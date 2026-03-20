; Go template injection for NATS server configuration files.
;
; Use for .conf.gotmpl files, or copy to your editor's query override
; directory to replace the default injections.scm.
;
; Neovim: copy to after/queries/nats_server_conf/injections.scm
; Zed:    configure in language extension

; {{ }} → Go templates
((template_interpolation_content) @injection.content
  (#set! injection.language "gotmpl"))

; ERB delimiters are unambiguous — still inject Ruby
((erb_interpolation_content) @injection.content
  (#set! injection.language "ruby"))

((erb_tag_content) @injection.content
  (#set! injection.language "ruby"))

((erb_comment_content) @injection.content
  (#set! injection.language "ruby"))
