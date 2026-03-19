# Design: Templating Language Support in tree-sitter-nats-server-conf

## Status

Draft — awaiting review.

## Problem

NATS server configuration files are frequently generated via templating engines
(Jinja2/Ansible, Go templates, ERB/Chef). The natural approach — using a
templating language's tree-sitter grammar as the "outer" grammar and injecting
`nats_server_conf` into the static regions — does not work well in practice.
Jinja2's tree-sitter grammar, for example, is not designed for arbitrary host
language injection; it treats everything between template delimiters as opaque
text, producing poor parse trees for the NATS config portions.

The alternative explored here is the **inverse approach**: extend the
`nats_server_conf` grammar to recognize template constructs as first-class
nodes, then use tree-sitter's injection mechanism to delegate template content
to the appropriate template language grammar.

This is the same pattern used by `tree-sitter-html` (which knows about
`<script>` and `<style>` and injects JavaScript/CSS) and `tree-sitter-htmldjango`
(HTML grammar extended with Django/Jinja template nodes).

## Assessment: Sane or Quagmire?

**Sane, with well-defined scope boundaries.**

The NATS config grammar is structurally simple — flat key-value pairs, blocks,
arrays, includes. This makes it far more tractable than extending a full
programming language grammar with template awareness. The key risks are:

1. **Scope creep**: trying to handle every possible template position
   (mid-identifier, mid-number, wrapping partial structures) leads to
   exponential grammar complexity. We mitigate this by explicitly scoping
   which positions are supported.

2. **`{{ }}` delimiter collision**: Jinja2 and Go templates both use `{{ }}`.
   We handle this through filetype-based disambiguation, not heuristics.

3. **Error recovery degradation**: template constructs that span structural
   boundaries (e.g., `{% if %}` wrapping an opening `{` but not its closing
   `}`) can confuse the parser. We handle this through careful node placement
   and accepting that some pathological template patterns will produce
   error-recovery parse trees.

## Supported Template Languages

| Language | Delimiters | Detection |
|----------|-----------|-----------|
| Jinja2 / Ansible | `{{ }}`, `{% %}`, `{# #}` | `{% %}` and `{# #}` are unambiguously Jinja. `{{ }}` is ambiguous with Go templates. |
| Go templates | `{{ }}` | Same delimiters as Jinja expressions. Distinguished by filetype. |
| ERB / eRuby | `<%= %>`, `<% %>`, `<%# %>` | Unambiguous — no collision with other supported languages. |

## Supported Template Positions

### 1. Statement-level (between pairs/blocks)

Template directives at the same level as key-value pairs and block definitions.
This is the most common pattern and the easiest to support.

```
{% for node in cluster_nodes %}
server_name: {{ node.name }}
port: {{ node.port }}
{% endfor %}
```

**Grammar change**: Add template node types as alternatives alongside
`_statement` and `comment` in `source_file` and `block` rules.

### 2. Inside values (composite fragments)

Template expressions embedded within values, producing composite value nodes.

```
cert_file: /etc/{{ hostname }}/cert.pem
listen: {{ bind_address }}:{{ port }}
```

**Grammar change**: The bare_string scanner stops when it encounters template
delimiters. Values become sequences of fragments and template expressions
via a new `templated_value` rule:

```
templated_value → (bare_string_fragment | template_interpolation)+
```

The `bare_string_fragment` is a new external token: same character set as
`bare_string` but stops at `{{`, `{%`, `<%` boundaries.

### 3. Wrapping structure (whole blocks and pairs-inside-blocks)

Template directives that wrap complete structural elements.

```
# Wrapping a whole block_definition
{% if enable_tls %}
tls {
  cert_file: /path/to/cert
  key_file: /path/to/key
}
{% endif %}

# Wrapping pairs inside a block
cluster {
  name: my-cluster
  {% if cluster_auth %}
  authorization {
    user: {{ cluster_user }}
    password: {{ cluster_pass }}
  }
  {% endif %}
}
```

**Grammar change**: Since template_tag nodes are allowed at statement-level
positions, this already works — `{% if %}` and `{% endif %}` are each
independent template_tag nodes. The parser doesn't need to understand that
they form a matched pair; that's the template language's concern.

### Explicitly NOT Supported (Out of Scope)

These patterns are rare enough in practice that supporting them isn't worth
the grammar complexity:

- **Template expressions as keys/identifiers**: `{{ key_name }}: value`
- **Template expressions inside numbers**: `port: {{ base_port }}`
  (this would be a `templated_value`, not a `number`)
- **Template directives splitting structural tokens**: `{% if x %}{{% endif %}`
  (template wrapping a single brace — pathological)
- **Nested template languages**: Jinja inside ERB or vice versa

## Node Types

### New External Tokens (scanner.c)

```c
enum TokenType {
  BARE_STRING,
  COMMENT,
  IDENTIFIER,
  BOOLEAN,
  // New template tokens:
  TEMPLATE_INTERPOLATION_CONTENT,  // content between {{ }}
  TEMPLATE_TAG_CONTENT,            // content between {% %}
  TEMPLATE_COMMENT_CONTENT,        // content between {# #}
  ERB_INTERPOLATION_CONTENT,       // content between <%= %>
  ERB_TAG_CONTENT,                 // content between <% %>
  ERB_COMMENT_CONTENT,             // content between <%# %>
  BARE_STRING_FRAGMENT,            // bare_string that stops at template delimiters
  TEMPLATE_DIALECT_MARKER,         // "# nats-template-dialect:" prefix
  TEMPLATE_DIALECT_NAME,           // dialect name word after the marker
};
```

### New Grammar Rules

```javascript
// Template interpolation: {{ expr }}
template_interpolation: $ => seq('{{', $.template_interpolation_content, '}}'),

// Template tag: {% stmt %}
template_tag: $ => seq('{%', optional('-'), $.template_tag_content, optional('-'), '%}'),

// Template comment: {# comment #}
template_comment: $ => seq('{#', $.template_comment_content, '#}'),

// ERB interpolation: <%= expr %>
erb_interpolation: $ => seq('<%=', $.erb_interpolation_content, '%>'),

// ERB tag: <% stmt %>
erb_tag: $ => seq('<%', optional('-'), $.erb_tag_content, optional('-'), '%>'),

// ERB comment: <%# comment %>
erb_comment: $ => seq('<%#', $.erb_comment_content, '%>'),

// Generic template construct (any of the above)
_template_directive: $ => choice(
  $.template_interpolation,
  $.template_tag,
  $.template_comment,
  $.erb_interpolation,
  $.erb_tag,
  $.erb_comment,
),

// Composite value with template fragments
templated_value: $ => prec.right(repeat1(
  choice($.bare_string_fragment, $._template_directive)
)),
```

### Updated Rules

```javascript
source_file: $ => repeat(choice($._statement, $.comment, $._template_directive)),

block: $ => seq('{', repeat(choice($._statement, $.comment, ',', $._template_directive)), '}'),

_value: $ => choice(
  $.string,
  $.number,
  $.boolean,
  $.variable_reference,
  $.array,
  $.templated_value,         // new
  $._template_directive,     // template expression as a complete value
),
```

## Scanner Changes (src/scanner.c)

### Template Delimiter Detection

The scanner needs to recognize template delimiters at two levels:

1. **As standalone tokens**: When `TEMPLATE_INTERPOLATION_CONTENT` etc. are
   valid symbols, scan the content between delimiters.

2. **As bare_string boundaries**: When scanning `BARE_STRING` or
   `BARE_STRING_FRAGMENT`, stop before template delimiters.

Key scanner additions:

```c
// Check for template delimiter at current position
// Returns: 0=none, 1={{ }}, 2={% %}, 3={# #}, 4=<%= %>, 5=<% %>, 6=<%# %>
static int detect_template_delimiter(TSLexer *lexer) {
  if (lexer->lookahead == '{') {
    // peek next char
    lexer->advance(lexer, false);
    if (lexer->lookahead == '{') return 1;  // {{
    if (lexer->lookahead == '%') return 2;  // {%
    if (lexer->lookahead == '#') return 3;  // {#
  }
  if (lexer->lookahead == '<') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '%') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '=') return 4;  // <%=
      if (lexer->lookahead == '#') return 6;  // <%#
      return 5;                                 // <%
    }
  }
  return 0;
}
```

**Critical change to bare_string scanning**: The `is_bare_char` function and
bare_string scanning loops must stop when encountering `{{`, `{%`, `{#`, or
`<%` sequences. This requires lookahead — a single `{` is still a valid
position to stop (it's a block opener), but `{` followed by `{`/`%`/`#`
means "template delimiter, stop here."

Since the bare_string scanner already doesn't consume `{` (it's not in
`is_bare_char`), the Jinja-style delimiters `{{`, `{%`, `{#` are
**already handled** — bare_string scanning naturally stops before `{`.

For ERB delimiters, `<` is also not in `is_bare_char`, so `<%` is also
already handled.

The only change needed in `is_bare_char` is: **none**. The current character
set already excludes `{` and `<`. The scanner naturally stops before
template delimiters.

However, we do need `BARE_STRING_FRAGMENT` as a separate token type because
`bare_string` is currently only valid in value positions, while
`bare_string_fragment` needs to work inside `templated_value` which has
different grammar context. The scanning logic is identical to `bare_string`.

## Template Dialect Configuration

Tree-sitter grammars compile to static C parsers — there is no runtime
configuration mechanism, no global variables, no feature flags. The grammar
itself has no concept of tunables. Every editor handles query customization
differently. This section describes the three complementary mechanisms we
use for dialect selection.

### Mechanism 1: In-file Modeline (`# nats-template-dialect:`)

The grammar recognizes a modeline comment as a first-class node:

```
# nats-template-dialect: jinja2
server_name: {{ hostname }}
port: {{ port }}
```

The modeline must be a comment matching the pattern
`# nats-template-dialect: <language>`. It produces a parse tree node:

```
(template_dialect_modeline
  (template_dialect_name))  ;; captures "jinja2"
```

The injection query uses the captured text as the injection language
dynamically — the same mechanism markdown grammars use for fenced code
block info strings:

```scheme
; Dynamic dialect from modeline — overrides all defaults when present
(template_dialect_modeline
  (template_dialect_name) @_dialect)

; When a modeline is present, use its value for all template injections.
; The #set! injection.language is overridden by the captured @_dialect text
; when the editor supports content-driven injection (Neovim, Zed do).
```

This is the **most portable** mechanism: it works in any editor that
supports tree-sitter content-driven injection, requires no editor-specific
configuration, and the modeline is self-documenting in the config file.

Recognized dialect names and their meanings:

| Modeline value | Injection language | Notes |
|----------------|-------------------|-------|
| `jinja2` or `jinja` | `jinja2` | Jinja2 / Ansible templates |
| `gotmpl` | `gotmpl` | Go `text/template` |
| `erb` | `ruby` | eRuby / ERB |
| `none` | *(no injection)* | Disable template language injection entirely |

The grammar node is `template_dialect_modeline`. The scanner recognizes
it only when the comment text matches the pattern exactly, so ordinary
comments like `# this is not a nats-template-dialect:` are unaffected.

### Mechanism 2: File Type Suffix

Editors map file extensions to grammars. By registering suffixed file types,
editors can activate both the grammar and an appropriate injection override:

| File pattern | Template language |
|-------------|-------------------|
| `*.conf` | Base grammar, default injection (jinja2) |
| `*.conf.j2`, `*.conf.jinja`, `*.conf.jinja2` | Jinja2 |
| `*.conf.gotmpl` | Go templates |
| `*.conf.erb` | ERB |

This requires per-editor filetype configuration (see Editor-Specific Notes)
but is the standard mechanism editors already use for language detection.

### Mechanism 3: Alternative Injection Files

We ship multiple injection query files. Users copy or symlink the
appropriate one into their editor's query override directory:

```
queries/
  injections.scm               # default: modeline-driven, falls back to jinja2
  injections-jinja.scm         # {{ }} → jinja2 (hardcoded, ignores modeline)
  injections-gotmpl.scm        # {{ }} → gotmpl (hardcoded, ignores modeline)
  injections-erb-only.scm      # ERB only; {{ }}/{% %} not injected
  injections-none.scm          # empty — disables all template injection
```

This is the **least portable** mechanism (requires manual per-editor setup)
but provides the most control and works even in editors with limited
injection support.

### Precedence

When multiple mechanisms are active:

1. **Modeline wins** — if `# nats-template-dialect:` is present in the file,
   it takes precedence (for editors that support content-driven injection).
2. **Editor query override** — alternative injection files or
   `after/queries/` overrides take effect next.
3. **Default injections.scm** — shipped default: `{% %}` → jinja2,
   `<% %>` → ruby, `{{ }}` → jinja2.

### Grammar Node for Modeline

The modeline is recognized by the external scanner as a specialized comment.
When the scanner encounters `#` followed by whitespace and the exact string
`nats-template-dialect:`, it produces a `TEMPLATE_DIALECT` token instead of
`COMMENT`. The grammar rule:

```javascript
template_dialect_modeline: $ => seq(
  '#',
  'nats-template-dialect:',
  field('dialect', $.template_dialect_name),
),

// Scanned by external scanner — captures the dialect name as a word
template_dialect_name: $ => /[a-zA-Z][a-zA-Z0-9_]*/,
```

The external scanner addition:

```c
enum TokenType {
  BARE_STRING,
  COMMENT,
  IDENTIFIER,
  BOOLEAN,
  // ...existing template tokens...
  TEMPLATE_DIALECT_MARKER,  // "# nats-template-dialect:" prefix
  TEMPLATE_DIALECT_NAME,    // the language name after the marker
};
```

The scanner recognizes the full `# nats-template-dialect:` prefix as a
single token, then the grammar's internal lexer picks up the dialect name.
This avoids the modeline being consumed as an ordinary comment.

## Injection Queries (queries/injections.scm)

The default `injections.scm` is **modeline-aware**: when a
`template_dialect_modeline` is present, its captured dialect name drives
injection for all template content nodes. When absent, static defaults
apply.

```scheme
; Default injections for template content.
; These inject the template language into the content nodes
; so editors can provide syntax highlighting for the template expressions.
;
; Override priority:
;   1. In-file modeline: # nats-template-dialect: <lang>
;   2. Editor query overrides (after/queries/ or alternative files)
;   3. These defaults

; --- Modeline-driven injection ---
; When the file contains "# nats-template-dialect: <lang>", use that
; language for all brace-style template content ({{ }} and {% %}).
; This uses content-driven injection: the captured node text becomes
; the injection language name.

(template_dialect_modeline
  (template_dialect_name) @injection.language)

; With modeline present, associate template content with the dialect.
; Editors that support combined injection (Neovim, Zed) will use the
; modeline language for all matching content nodes.

; --- Static fallback defaults (when no modeline) ---

; Jinja-style tags are unambiguously Jinja
((template_tag_content) @injection.content
  (#set! injection.language "jinja2"))

((template_comment_content) @injection.content
  (#set! injection.language "jinja2"))

; {{ }} is ambiguous between Jinja2 and Go templates.
; Default to jinja2; override via modeline or editor config.
((template_interpolation_content) @injection.content
  (#set! injection.language "jinja2"))

; ERB is unambiguously Ruby
((erb_interpolation_content) @injection.content
  (#set! injection.language "ruby"))

((erb_tag_content) @injection.content
  (#set! injection.language "ruby"))

((erb_comment_content) @injection.content
  (#set! injection.language "ruby"))
```

### Alternative: queries/injections-gotmpl.scm

```scheme
; Go template injection — use for .conf.gotmpl files
; or copy to your editor's query override directory.

((template_interpolation_content) @injection.content
  (#set! injection.language "gotmpl"))

; ERB nodes still inject Ruby (ERB delimiters are unambiguous)
((erb_interpolation_content) @injection.content
  (#set! injection.language "ruby"))
((erb_tag_content) @injection.content
  (#set! injection.language "ruby"))
((erb_comment_content) @injection.content
  (#set! injection.language "ruby"))
```

### Alternative: queries/injections-none.scm

```scheme
; Empty — disables all template language injection.
; Template delimiters are still highlighted via highlights.scm,
; but no inner language is injected.
```

## Highlight Queries (queries/highlights.scm additions)

```scheme
; Template delimiters
(template_interpolation ["{{" "}}"] @punctuation.special)
(template_tag ["{%" "%}"] @punctuation.special)
(template_comment ["{#" "#}"] @comment.special)
(erb_interpolation ["<%=" "%>"] @punctuation.special)
(erb_tag ["<%" "%>"] @punctuation.special)
(erb_comment ["<%#" "%>"] @comment.special)

; Template content fallback (when no injection language is available)
(template_interpolation_content) @embedded
(template_tag_content) @embedded
(erb_interpolation_content) @embedded
(erb_tag_content) @embedded
```

## File Type Associations

Update `tree-sitter.json` to register additional file extensions:

```json
"file-types": [
  "conf",
  { "suffix": ".conf.j2" },
  { "suffix": ".conf.jinja" },
  { "suffix": ".conf.jinja2" },
  { "suffix": ".conf.gotmpl" },
  { "suffix": ".conf.erb" }
]
```

Editors use these associations to:
1. Activate the `nats_server_conf` grammar for templated config files.
2. Optionally override the injection language based on the suffix
   (e.g., `.gotmpl` → inject `gotmpl` instead of `jinja2` for `{{ }}`).

## Editor-Specific Notes

### Easiest Path (All Editors)

Add a modeline to the config file:

```
# nats-template-dialect: jinja2
```

This works out-of-the-box in any editor that supports tree-sitter
content-driven injection. No editor-specific configuration required.

### Neovim (nvim-treesitter)

Neovim has mature injection support. The `injections.scm` file is loaded
automatically.

**Modeline**: Works natively. Neovim's injection engine supports
content-driven `@injection.language` captures.

**Filetype override** (alternative to modeline): add to `filetype.lua`:
```lua
vim.filetype.add({
  pattern = {
    [".*%.conf%.j2"] = "nats_server_conf",
    [".*%.conf%.jinja"] = "nats_server_conf",
    [".*%.conf%.jinja2"] = "nats_server_conf",
    [".*%.conf%.gotmpl"] = "nats_server_conf",
    [".*%.conf%.erb"] = "nats_server_conf",
  },
})
```

**Query override** (alternative to modeline): For Go templates without
a modeline, override in `after/queries/nats_server_conf/injections.scm`:
```scheme
; extends
; Override: inject gotmpl instead of jinja2 for {{ }}
((template_interpolation_content) @injection.content
  (#set! injection.language "gotmpl"))
```

Or copy `queries/injections-gotmpl.scm` to
`after/queries/nats_server_conf/injections.scm` (without `; extends`
to fully replace rather than extend).

### VS Code

VS Code uses tree-sitter via extensions (e.g., via `vscode-anycode` or
dedicated extensions). Injection support is more limited than Neovim/Zed.

**Modeline**: Depends on the extension's injection implementation. A
dedicated VS Code extension would need to explicitly support
content-driven injection from the modeline node.

**Filetype mapping**: Configured in the extension's `package.json`
contribution points. The extension should register all suffixed
file types.

**Alternative injection files**: Not directly supported by VS Code's
tree-sitter integration. The extension would need to bundle the
appropriate injection file or expose a setting.

### Zed

Zed has native tree-sitter support with injection queries.

**Modeline**: Works natively. Zed supports content-driven injection
the same way Neovim does.

**Filetype mapping**: Configured in Zed's `languages.toml` within
the language extension. The extension should register all suffixed
file types.

**Query override**: Users can override injection queries via
Zed's language extension config, similar to Neovim's approach.

## Implementation Plan

### Phase 1: Scanner + Grammar (this PR)

1. Add new token types to `enum TokenType` in scanner.c
2. Add template delimiter scanning logic to the external scanner
3. Add `bare_string_fragment` token (same scan logic as bare_string)
4. Add modeline scanning (`# nats-template-dialect:`) to the external scanner
5. Update grammar.js with new rules (template nodes + modeline)
6. Update highlights.scm with template delimiter highlighting
7. Create injections.scm (modeline-aware with static fallbacks)
8. Ship alternative injection files (gotmpl, erb-only, none)
9. Add test corpus files for each template pattern + modeline

### Phase 2: File Types + Editor Integration (follow-up)

1. Update tree-sitter.json with additional file-types
2. Write editor-specific setup documentation
3. Test with real-world templated configs
4. Publish updated grammar

### Phase 3: Refinement (based on feedback)

1. Handle trim markers (`{%-`, `-%}`, `<%-`, `-%>`)
2. Add indentation/fold support for template constructs
3. Consider `template_raw` blocks (`{% raw %}...{% endraw %}`)

## Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Template delimiters appear in non-template configs (e.g., `{#` in a comment) | Medium | Low | `{#` only triggers when followed by non-whitespace; `#` at line start is already captured as COMMENT before template scanning |
| Parse tree degradation for pathological templates | Low | Medium | Accept error recovery for unsupported patterns; document limitations |
| Performance impact of template delimiter checking | Low | Low | Checking is O(1) lookahead; only triggers on `{` or `<` characters |
| Breaking change to existing parse trees | None | N/A | Template nodes are additive; existing configs without template syntax parse identically |

## Open Questions

1. **Should `{#` be guarded?** In NATS config, `#` starts a comment. The
   sequence `{#` would currently not appear in valid non-templated config
   (since `{` starts a block and `#` starts a comment), but it's worth
   considering if any edge cases exist.

2. **Trim markers**: Jinja2 supports `{%-` and `-%}` for whitespace trimming.
   ERB supports `<%-` and `-%>`. Should these be recognized in Phase 1?
   (Proposed: yes, as they're common in real templates.)

3. **What tree-sitter grammar names do editors use for Jinja2 and Go
   templates?** This affects the `injection.language` strings. Neovim
   uses `jinja` (not `jinja2`). We should verify across target editors.

4. **Modeline placement**: Should `# nats-template-dialect:` be required
   at the top of the file (first non-blank line, like vim modelines)?
   Or anywhere (like Emacs file-local variables)? Top-of-file is simpler
   to implement and more predictable; anywhere is more flexible but
   requires scanning the full file.

5. **Modeline and content-driven injection interaction**: The exact
   mechanism for making the modeline's dialect name flow to all
   template content nodes needs prototyping. Markdown does this with
   fenced code blocks because the info string and content are in the
   same parent node. Our modeline is a sibling of template nodes, not
   a parent. We may need editor-specific query patterns, or we may
   need to restructure so that the modeline's dialect name is
   accessible from the injection query context. This is the riskiest
   part of the modeline design and needs a spike.

6. **`# nats-template-dialect: none`**: Should this disable template
   delimiter recognition entirely (scanner level), or just suppress
   injection (query level)? Query-level is simpler — delimiters still
   parse as template nodes but no language is injected, so they just
   get generic `@embedded` highlighting.
