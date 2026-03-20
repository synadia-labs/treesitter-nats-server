// External scanner for tree-sitter-nats-server-conf.
//
// Handles token types that require context-sensitive lexing:
//
// 1. bare_string: Unquoted string values containing special characters
//    like :, /, -, . (e.g., nats-route://host:6222, /var/lib/nats)
//
// 2. comment: # and // line comments. The scanner resolves ambiguity
//    with // inside URLs by preferring bare_string when both are valid.
//
// 3. identifier: Word tokens used as keys and block names.
//
// 4. boolean: true/false/yes/no/on/off values.
//
// 5. Template content tokens: opaque text between template delimiters
//    ({{ }}, {% %}, {# #}, <%= %>, <% %>, <%# %>).
//
// 6. bare_string_fragment: Like bare_string but used inside templated_value
//    composites.
//
// 7. template_dialect_name: The dialect word after "# nats-template-dialect:".
//
// All letter-starting tokens are external so the scanner can look ahead
// and produce the correct token type based on what follows.

#include "tree_sitter/parser.h"
#include <string.h>


enum TokenType {
  BARE_STRING,
  COMMENT,
  IDENTIFIER,
  BOOLEAN,
  // Template content tokens — opaque text between delimiters
  TEMPLATE_INTERPOLATION_CONTENT,  // content between {{ }}
  TEMPLATE_TAG_CONTENT,            // content between {% %} or {%- -%}
  TEMPLATE_COMMENT_CONTENT,        // content between {# #}
  ERB_INTERPOLATION_CONTENT,       // content between <%= %>
  ERB_TAG_CONTENT,                 // content between <% %> or <%- -%>
  ERB_COMMENT_CONTENT,             // content between <%# %>
  BARE_STRING_FRAGMENT,            // bare_string that stops at template delimiters
  TEMPLATE_DIALECT_NAME,           // dialect name word after modeline marker
};

// The NATS config lexer (conf/lex.go) is very permissive with identifiers:
// any character except whitespace and structural chars (= : { } [ ] ,) is valid.
// We mirror this by allowing ASCII letters, digits, underscore, hyphen, and
// all non-ASCII (Unicode) codepoints in identifiers.

static bool is_ident_start(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         c == '_' ||
         c >= 0x80;  // Unicode characters (e.g., prod-に)
}

static bool is_ident_char(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_' || c == '-' ||
         c >= 0x80;  // Unicode characters
}

static bool is_bare_char(int32_t c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_' || c == '-' || c == '.' || c == ':' ||
         c == '/' || c == '~' || c == '@' || c == '!' ||
         c == '%' || c == '^' || c == '+' || c == '*' ||
         c == '=' ||
         c >= 0x80;  // Unicode characters
}

static bool is_bare_only_start(int32_t c) {
  return c == '/' || c == '~' || c == '.' ||
         (c >= '0' && c <= '9');
}

static bool ci_eq(int32_t c, char ch) {
  if (c >= 'A' && c <= 'Z') c += 32;
  return c == ch;
}

// Check if the word "include" (the include directive keyword)
static bool is_include_keyword(const char *buf, int len) {
  return len == 7 &&
    buf[0] == 'i' && buf[1] == 'n' && buf[2] == 'c' &&
    buf[3] == 'l' && buf[4] == 'u' && buf[5] == 'd' && buf[6] == 'e';
}

// Check if a suffix after digits is a valid number suffix
// (size: k, kb, m, mb, g, gb, t, tb; duration: ns, us, ms, s, m, h, d)
static bool is_number_suffix(const char *suffix, int len) {
  if (len == 0) return true; // plain number
  if (len == 1) {
    char c = suffix[0] | 0x20; // lowercase
    return c == 'k' || c == 'm' || c == 'g' || c == 't' ||
           c == 's' || c == 'h' || c == 'd';
  }
  if (len == 2) {
    char c0 = suffix[0] | 0x20;
    char c1 = suffix[1] | 0x20;
    return (c0 == 'k' && c1 == 'b') || (c0 == 'm' && c1 == 'b') ||
           (c0 == 'g' && c1 == 'b') || (c0 == 't' && c1 == 'b') ||
           (c0 == 'n' && c1 == 's') || (c0 == 'u' && c1 == 's') ||
           (c0 == 'm' && c1 == 's');
  }
  return false;
}

// Check if a word buffer of given length matches a boolean keyword
static bool is_boolean_word(const char *buf, int len) {
  if (len == 2) {
    return (ci_eq(buf[0], 'o') && ci_eq(buf[1], 'n')) ||
           (ci_eq(buf[0], 'n') && ci_eq(buf[1], 'o'));
  }
  if (len == 3) {
    return (ci_eq(buf[0], 'y') && ci_eq(buf[1], 'e') && ci_eq(buf[2], 's')) ||
           (ci_eq(buf[0], 'o') && ci_eq(buf[1], 'f') && ci_eq(buf[2], 'f'));
  }
  if (len == 4) {
    return ci_eq(buf[0], 't') && ci_eq(buf[1], 'r') &&
           ci_eq(buf[2], 'u') && ci_eq(buf[3], 'e');
  }
  if (len == 5) {
    return ci_eq(buf[0], 'f') && ci_eq(buf[1], 'a') &&
           ci_eq(buf[2], 'l') && ci_eq(buf[3], 's') && ci_eq(buf[4], 'e');
  }
  return false;
}

// Scan opaque template content until a closing delimiter is found.
// closing1 and closing2 form the 2-char closing delimiter (e.g., '}' '}' or '%' '}').
// If allow_trim is true, an optional '-' before the closing delimiter is consumed
// (e.g., -%} or -%>).
static bool scan_template_content(TSLexer *lexer, int result_symbol,
                                  int32_t closing1, int32_t closing2,
                                  bool allow_trim) {
  // Skip leading whitespace (not part of content)
  while (!lexer->eof(lexer) &&
         (lexer->lookahead == ' ' || lexer->lookahead == '\t')) {
    lexer->advance(lexer, false);
  }
  lexer->mark_end(lexer);

  while (!lexer->eof(lexer)) {
    // Check for optional trim marker before closing delimiter
    if (allow_trim && lexer->lookahead == '-') {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (!lexer->eof(lexer) && lexer->lookahead == closing1) {
        lexer->advance(lexer, false);
        if (!lexer->eof(lexer) && lexer->lookahead == closing2) {
          // Found -closing — mark_end was set before the '-'
          lexer->result_symbol = result_symbol;
          return true;
        }
      }
      // Not a trim+close, continue scanning
      continue;
    }

    if (lexer->lookahead == closing1) {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (!lexer->eof(lexer) && lexer->lookahead == closing2) {
        // Found closing delimiter — mark_end is before it
        lexer->result_symbol = result_symbol;
        return true;
      }
      // Single char wasn't the start of the close delimiter, continue
      continue;
    }

    lexer->advance(lexer, false);
  }
  return false;  // EOF without finding closing delimiter
}

// Scan template content for ERB-style delimiters where the closing is %>.
// This shares logic with scan_template_content but closing is always '%' '>'.
static bool scan_erb_content(TSLexer *lexer, int result_symbol) {
  return scan_template_content(lexer, result_symbol, '%', '>', true);
}

// Check if the rest of the comment line (after '#') matches
// " nats-template-dialect:" and if so, leave the lexer positioned
// after the colon. Returns true if matched.
static bool check_modeline_prefix(TSLexer *lexer) {
  // We've already consumed '#'. Expect exactly:
  //   ' nats-template-dialect:'
  const char *expected = " nats-template-dialect:";
  for (int i = 0; expected[i] != '\0'; i++) {
    if (lexer->eof(lexer) || lexer->lookahead != (int32_t)expected[i]) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

void *tree_sitter_nats_server_conf_external_scanner_create(void) {
  return NULL;
}

void tree_sitter_nats_server_conf_external_scanner_destroy(void *p) {}

unsigned tree_sitter_nats_server_conf_external_scanner_serialize(
    void *p, char *buf) {
  return 0;
}

void tree_sitter_nats_server_conf_external_scanner_deserialize(
    void *p, const char *buf, unsigned len) {}

// Check if any template content token is valid in the current parse state.
static bool any_template_content_valid(const bool *valid_symbols) {
  return valid_symbols[TEMPLATE_INTERPOLATION_CONTENT] ||
         valid_symbols[TEMPLATE_TAG_CONTENT] ||
         valid_symbols[TEMPLATE_COMMENT_CONTENT] ||
         valid_symbols[ERB_INTERPOLATION_CONTENT] ||
         valid_symbols[ERB_TAG_CONTENT] ||
         valid_symbols[ERB_COMMENT_CONTENT];
}

bool tree_sitter_nats_server_conf_external_scanner_scan(
    void *payload, TSLexer *lexer, const bool *valid_symbols) {

  // --- Handle template content scanning ---
  // When the grammar has matched an opening delimiter (e.g., '{{'),
  // the parser expects content. We scan opaque text until the
  // matching closing delimiter.
  if (valid_symbols[TEMPLATE_INTERPOLATION_CONTENT] &&
      !valid_symbols[IDENTIFIER]) {
    return scan_template_content(lexer, TEMPLATE_INTERPOLATION_CONTENT,
                                 '}', '}', false);
  }
  if (valid_symbols[TEMPLATE_TAG_CONTENT] && !valid_symbols[IDENTIFIER]) {
    return scan_template_content(lexer, TEMPLATE_TAG_CONTENT,
                                 '%', '}', true);
  }
  if (valid_symbols[TEMPLATE_COMMENT_CONTENT] && !valid_symbols[IDENTIFIER]) {
    return scan_template_content(lexer, TEMPLATE_COMMENT_CONTENT,
                                 '#', '}', false);
  }
  if (valid_symbols[ERB_INTERPOLATION_CONTENT] && !valid_symbols[IDENTIFIER]) {
    return scan_erb_content(lexer, ERB_INTERPOLATION_CONTENT);
  }
  if (valid_symbols[ERB_TAG_CONTENT] && !valid_symbols[IDENTIFIER]) {
    return scan_erb_content(lexer, ERB_TAG_CONTENT);
  }
  if (valid_symbols[ERB_COMMENT_CONTENT] && !valid_symbols[IDENTIFIER]) {
    return scan_erb_content(lexer, ERB_COMMENT_CONTENT);
  }

  // --- Handle template dialect name ---
  // After the grammar matches "# nats-template-dialect:", we scan the
  // dialect name word.
  if (valid_symbols[TEMPLATE_DIALECT_NAME] && !valid_symbols[IDENTIFIER]) {
    // Skip whitespace after the colon
    while (!lexer->eof(lexer) &&
           (lexer->lookahead == ' ' || lexer->lookahead == '\t')) {
      lexer->advance(lexer, true);
    }
    if (lexer->eof(lexer)) return false;

    // Scan a word: [a-zA-Z][a-zA-Z0-9_-]*
    if (!((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
          (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z'))) {
      return false;
    }

    lexer->mark_end(lexer);
    while (!lexer->eof(lexer) &&
           ((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
            (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') ||
            (lexer->lookahead >= '0' && lexer->lookahead <= '9') ||
            lexer->lookahead == '_' || lexer->lookahead == '-')) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = TEMPLATE_DIALECT_NAME;
    return true;
  }

  // Skip whitespace — this is necessary because the internal lexer's
  // SKIP() mechanism doesn't re-invoke the external scanner after skipping.
  // By handling whitespace here, we ensure the scanner sees the first
  // non-whitespace character.
  while (!lexer->eof(lexer) &&
         (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
          lexer->lookahead == '\r' || lexer->lookahead == '\n')) {
    lexer->advance(lexer, true);  // true = skip (not part of token)
  }

  if (lexer->eof(lexer)) return false;

  int32_t c = lexer->lookahead;

  // --- Handle # comments and modeline ---
  // The modeline "# nats-template-dialect: <name>" is detected here.
  // If it matches, we return COMMENT for the prefix portion, letting
  // the grammar rule handle the modeline structure.
  // Actually: we produce a COMMENT token for normal # comments,
  // but for the modeline we need to NOT match so the grammar's
  // template_dialect_modeline rule can match instead.
  if (c == '#') {
    if (valid_symbols[COMMENT]) {
      // Peek ahead to check for modeline pattern
      lexer->advance(lexer, false);  // consume '#'

      // Save position — check if this is a modeline
      // We need to check " nats-template-dialect:" follows
      if (!lexer->eof(lexer) && lexer->lookahead == ' ') {
        // Tentatively scan for modeline prefix
        // But we can't "unread" in tree-sitter, so we use mark_end
        // to control what gets consumed.
        lexer->mark_end(lexer);  // mark after '#'

        const char *expected = " nats-template-dialect:";
        bool is_modeline = true;
        for (int i = 0; expected[i] != '\0'; i++) {
          if (lexer->eof(lexer) || lexer->lookahead != (int32_t)expected[i]) {
            is_modeline = false;
            break;
          }
          lexer->advance(lexer, false);
        }

        if (is_modeline) {
          // This IS a modeline. Return false so the grammar's
          // internal lexer can match the '#' and 'nats-template-dialect:'
          // literals in the template_dialect_modeline rule.
          return false;
        }

        // Not a modeline — consume the rest as a regular comment
        // We've already advanced past '#' and some chars. Read to EOL.
        while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
          lexer->advance(lexer, false);
        }
        lexer->mark_end(lexer);
        lexer->result_symbol = COMMENT;
        return true;
      }

      // Not starting with space after # — regular comment
      while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
      lexer->mark_end(lexer);
      lexer->result_symbol = COMMENT;
      return true;
    }
  }

  // --- Handle tokens starting with letters or _ ---
  if (is_ident_start(c) &&
      (valid_symbols[IDENTIFIER] || valid_symbols[BARE_STRING] ||
       valid_symbols[BOOLEAN] || valid_symbols[BARE_STRING_FRAGMENT])) {

    // Scan the identifier portion
    char word_buf[16];
    int word_len = 0;

    lexer->mark_end(lexer);
    while (!lexer->eof(lexer) && is_ident_char(lexer->lookahead)) {
      if (word_len < 15) {
        word_buf[word_len] = (char)lexer->lookahead;
      }
      word_len++;
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);

    // Check if bare_string chars follow (like - : / in URLs)
    if ((valid_symbols[BARE_STRING] || valid_symbols[BARE_STRING_FRAGMENT]) &&
        !lexer->eof(lexer) &&
        is_bare_char(lexer->lookahead) && !is_ident_char(lexer->lookahead) &&
        lexer->lookahead != '\n') {
      // Special chars follow — this is a bare_string (e.g., nats-route://...)
      while (!lexer->eof(lexer) && is_bare_char(lexer->lookahead) &&
             lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
      lexer->mark_end(lexer);
      // Prefer BARE_STRING over BARE_STRING_FRAGMENT when both are valid
      lexer->result_symbol = valid_symbols[BARE_STRING]
                               ? BARE_STRING : BARE_STRING_FRAGMENT;
      return true;
    }

    // Pure word — check if it's a boolean
    if (valid_symbols[BOOLEAN] && word_len <= 5 &&
        is_boolean_word(word_buf, word_len)) {
      lexer->result_symbol = BOOLEAN;
      return true;
    }

    // If it's the "include" keyword, return false so the internal
    // lexer can match the anon_sym_include token
    if (is_include_keyword(word_buf, word_len)) {
      return false;
    }

    // Otherwise it's an identifier
    if (valid_symbols[IDENTIFIER]) {
      lexer->result_symbol = IDENTIFIER;
      return true;
    }

    // Fallback: if bare_string or fragment is valid, return as such
    // Prefer BARE_STRING over BARE_STRING_FRAGMENT
    if (valid_symbols[BARE_STRING]) {
      lexer->result_symbol = BARE_STRING;
      return true;
    }
    if (valid_symbols[BARE_STRING_FRAGMENT]) {
      lexer->result_symbol = BARE_STRING_FRAGMENT;
      return true;
    }

    return false;
  }

  // --- Handle // comments ---
  // Must come before bare_only_start, because '/' is a bare_only_start char
  // and would otherwise greedily consume // comments as bare_strings.
  if (valid_symbols[COMMENT] && c == '/') {
    lexer->advance(lexer, false);
    if (!lexer->eof(lexer) && lexer->lookahead == '/') {
      lexer->advance(lexer, false);
      while (!lexer->eof(lexer) && lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
      lexer->mark_end(lexer);
      lexer->result_symbol = COMMENT;
      return true;
    }
    if (valid_symbols[BARE_STRING] || valid_symbols[BARE_STRING_FRAGMENT]) {
      while (!lexer->eof(lexer) && is_bare_char(lexer->lookahead) &&
             lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
      lexer->mark_end(lexer);
      lexer->result_symbol = valid_symbols[BARE_STRING]
                               ? BARE_STRING : BARE_STRING_FRAGMENT;
      return true;
    }
    return false;
  }

  // --- Handle bare strings starting with /, ~, ., or digits ---
  if ((valid_symbols[BARE_STRING] || valid_symbols[BARE_STRING_FRAGMENT]) &&
      is_bare_only_start(c)) {
    bool starts_with_digit = (c >= '0' && c <= '9');
    int dot_count = 0;
    int length = 0;
    bool has_non_digit_dot = false;
    char word_buf[16];

    lexer->mark_end(lexer);
    while (!lexer->eof(lexer) && is_bare_char(lexer->lookahead) &&
           lexer->lookahead != '\n') {
      int32_t ch = lexer->lookahead;
      if (ch == '.') dot_count++;
      if (ch != '.' && !(ch >= '0' && ch <= '9')) has_non_digit_dot = true;
      if (length < 15) word_buf[length] = (char)ch;
      length++;
      lexer->advance(lexer, false);
    }

    if (length == 0) return false;

    if (starts_with_digit) {
      // IP addresses have 2+ dots: 127.0.0.1, 0.0.0.0
      if (dot_count >= 2) {
        lexer->mark_end(lexer);
        lexer->result_symbol = valid_symbols[BARE_STRING]
                                 ? BARE_STRING : BARE_STRING_FRAGMENT;
        return true;
      }
      // Check if it's a number with a valid suffix (like 1GB, 64KB, 30s)
      // Find where the digits end and check the suffix
      if (has_non_digit_dot) {
        // Find the suffix part: skip digits and optional decimal point
        int suffix_start = 0;
        for (int i = 0; i < length; i++) {
          char ch = word_buf[i];
          if ((ch >= '0' && ch <= '9') || ch == '.') {
            suffix_start = i + 1;
          } else {
            break;
          }
        }
        int suffix_len = (length < 15 ? length : 15) - suffix_start;
        if (suffix_len <= 2 && is_number_suffix(word_buf + suffix_start, suffix_len)) {
          // This is a number with suffix — don't match as bare_string
          return false;
        }
        // Has non-suffix chars — it's a bare_string
        lexer->mark_end(lexer);
        lexer->result_symbol = valid_symbols[BARE_STRING]
                                 ? BARE_STRING : BARE_STRING_FRAGMENT;
        return true;
      }
      return false;
    }

    lexer->mark_end(lexer);
    lexer->result_symbol = valid_symbols[BARE_STRING]
                             ? BARE_STRING : BARE_STRING_FRAGMENT;
    return true;
  }

  // --- Handle bare_string_fragment for mid-value chars ---
  // Inside a templated_value, chars like : @ ! can appear between
  // template directives (e.g., {{ host }}:{{ port }}). These chars
  // are valid bare_char but don't start identifiers or bare_only_start.
  // We exclude '-' followed by a digit (negative numbers) to avoid
  // consuming the sign of a negative integer.
  if (valid_symbols[BARE_STRING_FRAGMENT] && !valid_symbols[BARE_STRING] &&
      is_bare_char(c) && c != '\n') {
    lexer->mark_end(lexer);
    while (!lexer->eof(lexer) && is_bare_char(lexer->lookahead) &&
           lexer->lookahead != '\n') {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = BARE_STRING_FRAGMENT;
    return true;
  }

  return false;
}
