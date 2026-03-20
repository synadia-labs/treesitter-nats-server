/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

// Tree-sitter grammar for NATS server configuration files.
//
// The NATS server config format is a JSON-like superset that supports:
// - Multiple comment styles (# and //)
// - Three key-value separators (=, :, whitespace)
// - Block definitions with { }
// - Include directives
// - Variable definitions and $references
// - Unquoted strings, booleans, numbers with size/duration suffixes
//
// Template language support:
// - Jinja2/Ansible: {{ }}, {% %}, {# #}
// - Go templates: {{ }} (same delimiters; distinguished by filetype)
// - ERB/eRuby: <%= %>, <% %>, <%# %>
// - In-file dialect modeline: # nats-template-dialect: <name>
//
// Reference: https://docs.nats.io/running-a-nats-service/configuration

module.exports = grammar({
  name: "nats_server_conf",

  extras: ($) => [/\s/],

  conflicts: ($) => [
    // After seeing bare_string, the parser can't tell if it's a string
    // value or the start of a templated_value until it sees what follows.
    // GLR resolves this: if a template interpolation follows, it's
    // templated_value; otherwise it's string.
    [$.string, $.templated_value],
  ],

  // External tokens handled by src/scanner.c:
  // - bare_string: unquoted values like nats-route://host:6222
  // - comment: # and // comments (external to resolve // in URLs)
  // - identifier: keys and block names
  // - boolean: true/false/yes/no/on/off
  // - template content tokens: opaque text between template delimiters
  // - bare_string_fragment: bare_string inside templated_value composites
  // - template_dialect_name: dialect word after modeline marker
  externals: ($) => [
    $.bare_string,
    $.comment,
    $.identifier,
    $.boolean,
    $.template_interpolation_content,
    $.template_tag_content,
    $.template_comment_content,
    $.erb_interpolation_content,
    $.erb_tag_content,
    $.erb_comment_content,
    $.bare_string_fragment,
    $.template_dialect_name,
  ],

  rules: {
    source_file: ($) =>
      repeat(
        choice($._statement, $.comment, $._template_directive,
               $.template_dialect_modeline)
      ),

    _statement: ($) =>
      choice($.include_directive, $.block_definition, $.pair),

    // ---------------------------------------------------------------
    // Include directive: include ./path/to/file.conf
    // ---------------------------------------------------------------
    include_directive: ($) =>
      seq(
        field("keyword", alias("include", $.include_keyword)),
        field("path", $.string)
      ),

    // ---------------------------------------------------------------
    // Block definition: identifier [= | :] { ... }
    // ---------------------------------------------------------------
    block_definition: ($) =>
      prec(
        1,
        seq(
          field("name", $.identifier),
          optional(choice("=", ":")),
          field("body", $.block)
        )
      ),

    block: ($) =>
      seq(
        "{",
        repeat(choice($._statement, $.comment, ",", $._template_directive)),
        "}"
      ),

    // ---------------------------------------------------------------
    // Key-value pair
    // Requires = or : separator to avoid ambiguity with bare_string values.
    // The whitespace-only separator (key value) is rare in practice;
    // the include directive is the primary user of that pattern.
    // ---------------------------------------------------------------
    pair: ($) =>
      seq(
        field("key", $.identifier),
        choice("=", ":"),
        field("value", $._value)
      ),

    // ---------------------------------------------------------------
    // Values
    // ---------------------------------------------------------------
    _value: ($) =>
      choice(
        $.string,
        $.number,
        $.boolean,
        $.variable_reference,
        $.array,
        $.templated_value
      ),

    // Strings: quoted or unquoted (bare)
    string: ($) =>
      choice($._quoted_string, $.bare_string),

    _quoted_string: ($) =>
      token(seq('"', repeat(choice(/[^"\\]/, /\\./)), '"')),

    // Numbers: integers and floats, with optional size/duration suffixes
    number: ($) =>
      token(
        seq(
          optional("-"),
          choice(
            seq(/[0-9]+/, ".", /[0-9]+/),
            /[0-9]+/
          ),
          optional(
            token.immediate(
              choice(
                /[kK][bB]?/,
                /[mM][bB]/,
                /[gG][bB]?/,
                /[tT][bB]?/,
                /[nN][sS]/,
                /[uU][sS]/,
                "µs",
                /[mM][sS]/,
                /[sSmMhHdD]/
              )
            )
          )
        )
      ),

    // Variable reference: $VARNAME
    variable_reference: ($) =>
      token(seq("$", /[a-zA-Z_][a-zA-Z0-9_]*/)),

    // Array: [val1, val2, ...]
    array: ($) =>
      seq(
        "[",
        repeat(choice($._array_value, ",", $.comment)),
        "]"
      ),

    _array_value: ($) =>
      choice(
        $.string,
        $.number,
        $.boolean,
        $.variable_reference,
        $.array,
        $.block
      ),

    // ---------------------------------------------------------------
    // Template constructs
    // ---------------------------------------------------------------

    // All template constructs — used at statement level in source_file
    // and block bodies where any template directive (tags, interpolations,
    // comments) is valid.
    _template_directive: ($) =>
      choice(
        $.template_interpolation,
        $.template_tag,
        $.template_comment,
        $.erb_interpolation,
        $.erb_tag,
        $.erb_comment
      ),

    // Template interpolation constructs only — used in templated_value
    // where only value-producing expressions ({{ }}, <%= %>) are valid.
    // Statement-level constructs ({% %}, <% %>) and comments ({# #},
    // <%# %>) belong at statement level, not inside values.
    _template_value_interpolation: ($) =>
      choice(
        $.template_interpolation,
        $.erb_interpolation
      ),

    // Jinja2/Go template interpolation: {{ expr }}
    template_interpolation: ($) =>
      seq("{{", optional($.template_interpolation_content), "}}"),

    // Jinja2 template tag: {% stmt %} or {%- stmt -%}
    template_tag: ($) =>
      seq(
        choice("{%", "{%-"),
        optional($.template_tag_content),
        choice("%}", "-%}")
      ),

    // Jinja2 template comment: {# comment #}
    template_comment: ($) =>
      seq("{#", optional($.template_comment_content), "#}"),

    // ERB interpolation: <%= expr %>
    erb_interpolation: ($) =>
      seq("<%=", optional($.erb_interpolation_content), choice("%>", "-%>")),

    // ERB tag: <% stmt %> or <%- stmt -%>
    erb_tag: ($) =>
      seq(
        choice("<%", "<%-"),
        optional($.erb_tag_content),
        choice("%>", "-%>")
      ),

    // ERB comment: <%# comment %>
    erb_comment: ($) =>
      seq("<%#", optional($.erb_comment_content), choice("%>", "-%>")),

    // Composite value with template interpolations interspersed:
    // e.g., /etc/{{ hostname }}/cert.pem
    //        {{ bind_address }}:{{ port }}
    // Must contain at least one interpolation; otherwise it's a plain
    // string/bare_string. Only interpolation constructs ({{ }}, <%= %>)
    // are allowed — statement-level tags ({% %}, <% %>) remain at
    // statement level and don't extend into values.
    templated_value: ($) =>
      prec.right(
        seq(
          optional($.bare_string),
          $._template_value_interpolation,
          repeat(choice($.bare_string_fragment, $._template_value_interpolation))
        )
      ),

    // ---------------------------------------------------------------
    // Template dialect modeline
    // ---------------------------------------------------------------
    // # nats-template-dialect: jinja2
    template_dialect_modeline: ($) =>
      seq(
        "#",
        "nats-template-dialect:",
        field("dialect", $.template_dialect_name)
      ),
  },
});
