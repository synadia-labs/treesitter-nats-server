# tree-sitter-nats-server-conf

A [tree-sitter](https://tree-sitter.github.io/) grammar for [NATS server](https://nats.io/) configuration files.

Provides syntax highlighting, code folding, indentation, and structural navigation for `.conf` files used by `nats-server`.

## Features

The grammar handles the full NATS server configuration format:

- **Comments**: `#` and `//` line comments
- **Key-value pairs**: with `=` or `:` separators
- **Blocks**: `cluster { }`, `jetstream { }`, `tls { }`, etc.
- **Nested blocks**: arbitrary nesting depth
- **Arrays**: `[value1, value2]` with commas or newlines
- **Strings**: quoted `"..."` with escape sequences, and unquoted bare strings (URLs, paths, IP addresses)
- **Numbers**: integers, floats, with size suffixes (`KB`, `MB`, `GB`, `TB`) and duration suffixes (`ns`, `us`, `ms`, `s`, `m`, `h`, `d`)
- **Booleans**: `true`/`false`, `yes`/`no`, `on`/`off` (case insensitive)
- **Variables**: `$VARNAME` references
- **Include directives**: `include ./path/to/file.conf`

## Quick Start

```bash
# Install dependencies
npm install

# Generate the parser
tree-sitter generate

# Run tests
tree-sitter test

# Parse a file
tree-sitter parse examples/basic.conf
```

## Building

```bash
# Build the shared library
make

# Install system-wide
sudo make install
```

## Editor Integration

- [Neovim](docs/editors/neovim.md) — nvim-treesitter setup with highlighting, folding, and indentation
- [Helix](docs/editors/helix.md) — languages.toml configuration and query installation
- [VS Code](docs/editors/vscode.md) — Extension setup with TextMate grammar fallback
- [Emacs](docs/editors/emacs.md) — Built-in treesit (29+) and emacs-tree-sitter setup
- [Zed](docs/editors/zed.md) — Extension with tree-sitter grammar and query files
- [Nova](docs/editors/nova.md) — Panic Nova extension with syntax definition

## Grammar Structure

```
source_file
├── comment              # or // line comments
├── include_directive    include <path>
├── pair                 key = value  or  key: value
│   ├── key              identifier
│   └── value
│       ├── string       "quoted" or bare/unquoted
│       ├── number       42, 3.14, 1GB, 30s
│       ├── boolean      true/false/yes/no/on/off
│       ├── variable_reference  $VARNAME
│       └── array        [...]
└── block_definition     name { ... }
    └── block            { statements... }
```

## Language Bindings

Bindings are available for:

- **C** — `bindings/c/`
- **Go** — `bindings/go/` + `go.mod`
- **Node.js** — `bindings/node/`
- **Python** — `bindings/python/` + `pyproject.toml`
- **Rust** — `bindings/rust/` + `Cargo.toml`
- **Swift** — `bindings/swift/` + `Package.swift`

## Examples

See the [`examples/`](examples/) directory for sample configuration files:

- `basic.conf` — Simple server setup
- `cluster.conf` — Cluster with routes and TLS
- `jetstream.conf` — JetStream with accounts
- `full.conf` — Comprehensive configuration

## License

MIT — see [LICENSE](LICENSE).
