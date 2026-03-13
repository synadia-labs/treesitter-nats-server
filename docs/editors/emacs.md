# Emacs Integration

This guide explains how to add NATS server configuration file support to Emacs using [tree-sitter](https://tree-sitter.github.io/) via the built-in `treesit` module or the [emacs-tree-sitter](https://github.com/emacs-tree-sitter/elisp-tree-sitter) package.

## Option 1: Built-in treesit (Emacs 29+)

Emacs 29 introduced native tree-sitter support through the `treesit` module.

### Prerequisites

- Emacs 29+ built with tree-sitter support
- A C compiler (gcc or clang)

### 1. Build and install the shared library

```bash
git clone https://github.com/synadia-labs/treesitter-nats-server.git
cd treesitter-nats-server
make

# Install into Emacs's tree-sitter directory
mkdir -p ~/.emacs.d/tree-sitter
cp libtree-sitter-nats_server_conf.so ~/.emacs.d/tree-sitter/
```

On macOS the shared library extension is `.dylib` instead of `.so`.

### 2. Create a major mode

Add the following to your Emacs configuration (e.g., `~/.emacs.d/init.el` or `~/.config/emacs/init.el`):

```elisp
(require 'treesit)

(define-derived-mode nats-server-conf-ts-mode prog-mode "NATS Conf"
  "Major mode for editing NATS server configuration files, powered by tree-sitter."
  (when (treesit-ready-p 'nats_server_conf)
    (treesit-parser-create 'nats_server_conf)
    (setq-local treesit-font-lock-feature-list
                '((comment)
                  (keyword string)
                  (number boolean variable constant)
                  (bracket delimiter)))
    (setq-local treesit-font-lock-settings
                (treesit-font-lock-rules
                 :language 'nats_server_conf
                 :feature 'comment
                 '((comment) @font-lock-comment-face)

                 :language 'nats_server_conf
                 :feature 'keyword
                 '((include_directive "include" @font-lock-keyword-face))

                 :language 'nats_server_conf
                 :feature 'string
                 '((string) @font-lock-string-face)

                 :language 'nats_server_conf
                 :feature 'number
                 '((number) @font-lock-number-face)

                 :language 'nats_server_conf
                 :feature 'boolean
                 '((boolean) @font-lock-constant-face)

                 :language 'nats_server_conf
                 :feature 'variable
                 '((variable_reference) @font-lock-variable-use-face)

                 :language 'nats_server_conf
                 :feature 'constant
                 '((block_definition name: (_) @font-lock-type-face)
                   (pair key: (_) @font-lock-property-name-face))

                 :language 'nats_server_conf
                 :feature 'bracket
                 '((["(" ")" "[" "]" "{" "}"]) @font-lock-bracket-face)

                 :language 'nats_server_conf
                 :feature 'delimiter
                 '((["=" ":"]) @font-lock-delimiter-face)))

    (setq-local comment-start "# ")
    (setq-local comment-end "")
    (setq-local treesit-simple-indent-rules
                `((nats_server_conf
                   ((parent-is "block") parent-bol 2)
                   ((parent-is "array") parent-bol 2)
                   ((parent-is "source_file") column-0 0))))
    (treesit-major-mode-setup)))
```

### 3. Set up filetype detection

Add to your Emacs configuration:

```elisp
(add-to-list 'auto-mode-alist '("nats-server.*\\.conf\\'" . nats-server-conf-ts-mode))
(add-to-list 'auto-mode-alist '("nats\\.conf\\'" . nats-server-conf-ts-mode))
```

### 4. Verify

Open a NATS configuration file. You should see syntax highlighting and `NATS Conf` in the mode line:

```
M-x nats-server-conf-ts-mode
```

## Option 2: emacs-tree-sitter (Emacs 27–28)

For older Emacs versions, use the [emacs-tree-sitter](https://github.com/emacs-tree-sitter/elisp-tree-sitter) package.

### Prerequisites

- Emacs 27+
- [tree-sitter](https://github.com/emacs-tree-sitter/elisp-tree-sitter) and [tree-sitter-langs](https://github.com/emacs-tree-sitter/tree-sitter-langs) packages

### 1. Install the packages

Using `use-package`:

```elisp
(use-package tree-sitter
  :ensure t)
(use-package tree-sitter-langs
  :ensure t)
```

### 2. Build and register the grammar

Build the shared library as described in Option 1, then register it:

```elisp
(tree-sitter-load 'nats_server_conf "path/to/libtree-sitter-nats_server_conf")
(add-to-list 'tree-sitter-major-mode-language-alist
             '(nats-server-conf-mode . nats_server_conf))
```

### 3. Create a basic major mode

```elisp
(define-derived-mode nats-server-conf-mode prog-mode "NATS Conf"
  "Major mode for editing NATS server configuration files."
  (setq-local comment-start "# ")
  (setq-local comment-end "")
  (tree-sitter-hl-mode))
```

### 4. Set up filetype detection

```elisp
(add-to-list 'auto-mode-alist '("nats-server.*\\.conf\\'" . nats-server-conf-mode))
(add-to-list 'auto-mode-alist '("nats\\.conf\\'" . nats-server-conf-mode))
```

## Troubleshooting

- **"Cannot find shared library"**: Ensure the `.so` (or `.dylib`) file is in `~/.emacs.d/tree-sitter/` or a directory listed in `treesit-extra-load-path`
- **No highlighting**: Verify the mode is active with `M-x describe-mode` and that tree-sitter is compiled in with `(treesit-available-p)`
- **Wrong mode**: Check that `auto-mode-alist` entries match your file naming convention
