; ERB-only injection for NATS server configuration files.
;
; Only injects Ruby for ERB delimiters (<%= %>, <% %>, <%# %>).
; Brace-style delimiters ({{ }}, {% %}, {# #}) are NOT injected —
; they still parse as template nodes but get only generic highlighting.
;
; Use when your configs mix ERB with brace-style syntax that isn't
; a template language (unlikely but possible).

((erb_interpolation_content) @injection.content
  (#set! injection.language "ruby"))

((erb_tag_content) @injection.content
  (#set! injection.language "ruby"))

((erb_comment_content) @injection.content
  (#set! injection.language "ruby"))
