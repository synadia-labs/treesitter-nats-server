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
