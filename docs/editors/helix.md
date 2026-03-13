# Helix Integration

This guide explains how to add NATS server configuration file support to [Helix](https://helix-editor.com/).

## Prerequisites

- Helix 24.03+ (with tree-sitter support)
- A C compiler (gcc or clang)
- `tree-sitter` CLI (for building the grammar)

## Setup

### 1. Build the grammar

Clone this repository and build the shared library:

```bash
git clone https://github.com/philpennock/tree-sitter-nats-server-conf.git
cd tree-sitter-nats-server-conf
tree-sitter generate
cc -shared -fPIC -I src src/parser.c src/scanner.c -o nats_server_conf.so
```

### 2. Configure the language

Add the following to your Helix `languages.toml` file (usually at `~/.config/helix/languages.toml`):

```toml
[[language]]
name = "nats-server-conf"
scope = "source.nats-server-conf"
injection-regex = "nats"
file-types = [
  { glob = "*nats-server*.conf" },
  { glob = "*nats*.conf" },
]
comment-token = "#"
indent = { tab-width = 2, unit = "  " }
roots = []

[[grammar]]
name = "nats_server_conf"
source = { git = "https://github.com/philpennock/tree-sitter-nats-server-conf", rev = "main" }
```

### 3. Fetch and build the grammar

```bash
hx --grammar fetch
hx --grammar build
```

### 4. Install query files

Copy the query files to the Helix runtime directory:

```bash
QUERIES_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/helix/runtime/queries/nats_server_conf"
mkdir -p "$QUERIES_DIR"
cp queries/highlights.scm "$QUERIES_DIR/"
cp queries/indents.scm "$QUERIES_DIR/"
cp queries/locals.scm "$QUERIES_DIR/"
```

Note: Helix uses `textobjects.scm` instead of `folds.scm`. You can create one if needed:

```bash
cat > "$QUERIES_DIR/textobjects.scm" << 'EOF'
(block) @function.inside
(block_definition) @function.around
(array) @parameter.inside
EOF
```

### 5. Verify

Open a NATS configuration file:

```bash
hx examples/basic.conf
```

You should see syntax highlighting. Use `:tree-sitter-subtree` to inspect the parse tree at the cursor position.

## Troubleshooting

- **No highlighting**: Check that the query files are in the correct directory with `ls ~/.config/helix/runtime/queries/nats_server_conf/`
- **Grammar build fails**: Ensure you have a C compiler and that the grammar source is accessible
- **Wrong filetype**: Verify with `:lang` that the file is detected as `nats-server-conf`
