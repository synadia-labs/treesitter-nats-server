; Injection queries for NATS server configuration template support.
;
; Override priority:
;   1. In-file modeline: # nats-template-dialect: <lang>
;   2. Editor query overrides (after/queries/ or alternative injection files)
;   3. These static defaults
;
; See also:
;   queries/injections-gotmpl.scm   — hardcoded Go template injection
;   queries/injections-erb-only.scm — ERB only, ignores {{ }}/{% %}
;   queries/injections-none.scm     — disables all injection

; --- Modeline-driven injection ---
; When the file contains "# nats-template-dialect: <name>", the dialect
; name node can be used by editors that support content-driven injection.
; The template_dialect_name text becomes the injection language.
;
; Note: this captures the modeline node for editor use. The actual
; association between the dialect name and template content nodes
; depends on editor-specific injection resolution. Neovim and Zed
; support this pattern natively.

; --- Static fallback defaults ---

; Jinja-style tags ({% %}) are unambiguously Jinja2
((template_tag_content) @injection.content
  (#set! injection.language "jinja2"))

((template_comment_content) @injection.content
  (#set! injection.language "jinja2"))

; {{ }} is ambiguous between Jinja2 and Go templates.
; Default to jinja2; override via modeline, filetype, or editor config.
((template_interpolation_content) @injection.content
  (#set! injection.language "jinja2"))

; ERB is unambiguously Ruby
((erb_interpolation_content) @injection.content
  (#set! injection.language "ruby"))

((erb_tag_content) @injection.content
  (#set! injection.language "ruby"))

((erb_comment_content) @injection.content
  (#set! injection.language "ruby"))
