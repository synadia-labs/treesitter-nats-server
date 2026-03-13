# Zed Integration

This guide explains how to add NATS server configuration file support to [Zed](https://zed.dev/).

Zed has first-class tree-sitter support. You can add NATS server configuration support by creating a Zed extension.

## Prerequisites

- Zed 0.131+ (with extension support)
- Rust toolchain (for building the extension)

## Setup

### 1. Create the extension directory

```bash
mkdir -p ~/.local/share/zed/extensions/nats-server-conf
cd ~/.local/share/zed/extensions/nats-server-conf
```

On macOS, use `~/Library/Application Support/Zed/extensions/nats-server-conf` instead.

### 2. Create `extension.toml`

```toml
id = "nats-server-conf"
name = "NATS Server Configuration"
description = "Tree-sitter grammar and syntax highlighting for NATS server configuration files."
version = "0.1.0"
schema_version = 1
authors = ["Your Name <your@email.com>"]
repository = "https://github.com/synadia-labs/treesitter-nats-server"

[grammars.nats_server_conf]
repository = "https://github.com/synadia-labs/treesitter-nats-server"
rev = "main"
```

### 3. Create the language configuration

Create `languages/nats-server-conf/config.toml`:

```bash
mkdir -p languages/nats-server-conf
```

```toml
name = "NATS Server Config"
grammar = "nats_server_conf"
path_suffixes = ["conf"]
line_comments = ["# ", "// "]
block_comment = []
brackets = [
  { start = "{", end = "}", close = true, newline = true },
  { start = "[", end = "]", close = true, newline = false },
  { start = "\"", end = "\"", close = true, newline = false, not_in = ["string"] },
]
```

### 4. Install query files

Copy the query files from this repository into the extension:

```bash
QUERIES_DIR="languages/nats-server-conf"
cp /path/to/treesitter-nats-server/queries/highlights.scm "$QUERIES_DIR/"
cp /path/to/treesitter-nats-server/queries/folds.scm "$QUERIES_DIR/"
cp /path/to/treesitter-nats-server/queries/indents.scm "$QUERIES_DIR/"
```

### 5. Add file detection

Create `languages/nats-server-conf/file_types.toml`:

```toml
path_suffixes = ["conf"]
file_stems = ["nats-server", "nats"]

[[path_glob]]
glob = "*nats*.conf"
```

### 6. Rebuild and reload

Restart Zed or use the command palette:

```
zed: reload extensions
```

### 7. Verify

Open a NATS configuration file. You should see syntax highlighting and the language indicator showing "NATS Server Config" in the status bar.

## Alternative: Dev Extension (for development)

If you want to iterate on the extension during development:

1. Clone this repository
2. In Zed, open the command palette and run `zed: install dev extension`
3. Select the cloned repository directory
4. Zed will build and load the extension automatically

This approach auto-reloads the extension when you make changes.

## Troubleshooting

- **No highlighting**: Check that the query files are present in `languages/nats-server-conf/` and that the grammar name matches `nats_server_conf`
- **Extension not loading**: Open `zed: open log` from the command palette and look for extension-related errors
- **Wrong language detected**: `.conf` is a generic extension. Zed matches by file path glob, so ensure your file name contains `nats`
