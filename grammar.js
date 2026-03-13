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
// Reference: https://docs.nats.io/running-a-nats-service/configuration

module.exports = grammar({
  name: "nats_server_conf",

  extras: ($) => [/\s/],

  // External tokens handled by src/scanner.c:
  // - bare_string: unquoted values like nats-route://host:6222
  // - comment: # and // comments (external to resolve // in URLs)
  // - identifier: keys and block names
  // - boolean: true/false/yes/no/on/off
  externals: ($) => [
    $.bare_string,
    $.comment,
    $.identifier,
    $.boolean,
  ],

  rules: {
    source_file: ($) => repeat(choice($._statement, $.comment)),

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
        repeat(choice($._statement, $.comment, ",")),
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
        $.array
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
  },
});
