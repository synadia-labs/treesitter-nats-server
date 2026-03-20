; Syntax highlighting queries for NATS server configuration files

; Comments
(comment) @comment

; Include directive keyword
(include_keyword) @keyword

; Block names (cluster, jetstream, tls, etc.)
(block_definition
  name: (identifier) @type)

; Key names in pairs
(pair
  key: (identifier) @property)

; Strings
(string) @string

; Numbers
(number) @number

; Booleans
(boolean) @constant.builtin

; Variable references ($VAR)
(variable_reference) @variable

; Punctuation
"{" @punctuation.bracket
"}" @punctuation.bracket
"[" @punctuation.bracket
"]" @punctuation.bracket

"=" @punctuation.delimiter
":" @punctuation.delimiter
"," @punctuation.delimiter

; Template delimiters — Jinja2/Go template style
(template_interpolation ["{{" "}}"] @punctuation.special)
(template_tag ["{%" "{%-" "%}" "-%}"] @punctuation.special)
(template_comment ["{#" "#}"] @comment.special)

; Template delimiters — ERB style
(erb_interpolation ["<%=" "%>" "-%>"] @punctuation.special)
(erb_tag ["<%" "<%-" "%>" "-%>"] @punctuation.special)
(erb_comment ["<%#" "%>" "-%>"] @comment.special)

; Template content fallback (when no injection language is available)
(template_interpolation_content) @embedded
(template_tag_content) @embedded
(erb_interpolation_content) @embedded
(erb_tag_content) @embedded

; Template comment content
(template_comment_content) @comment
(erb_comment_content) @comment

; Bare string fragments in templated values
(bare_string_fragment) @string

; Template dialect modeline
(template_dialect_modeline) @comment.special
(template_dialect_modeline "#" @comment.special)
(template_dialect_modeline "nats-template-dialect:" @comment.special)
(template_dialect_name) @string.special
