# VS Code Integration

This guide explains how to add NATS server configuration file support to Visual Studio Code.

## Option 1: Using vscode-anycode (Quick Setup)

The [anycode extension](https://marketplace.visualstudio.com/items?itemName=nicolo-ribaudo.vscode-anycode) provides tree-sitter support in VS Code.

This relies upon the tree-sitter command (install however you wish, we show
this assuming the `cargo` command) and the `tree-sitter build --wasm` step
relies upon being able to run an OCI image (`emscripten/emsdk`); this will
implicitly require `docker` or `podman` or some other command which
tree-sitter supports for that invocation.

### Setup

1. Install the anycode extension
2. Clone this repository:
   ```bash
   git clone https://github.com/synadia-labs/treesitter-nats-server.git
   ```
3. Build the WASM module:
   ```bash
   cd treesitter-nats-server
   command -v tree-sitter || cargo install tree-sitter-cli
   tree-sitter build --wasm
   ```
4. Configure anycode in your VS Code `settings.json`:
   ```json
   {
     "anycode.language.features": {
       "nats-server-conf": {
         "folding": true,
         "highlights": true
       }
     }
   }
   ```

## Option 2: Creating a VS Code Extension

For a more polished experience, you can create a dedicated extension.

### 1. Scaffold the extension

```bash
mkdir vscode-nats-server-conf
cd vscode-nats-server-conf
npm init -y
```

### 2. Create `package.json`

```json
{
  "name": "vscode-nats-server-conf",
  "displayName": "NATS Server Configuration",
  "description": "Syntax highlighting for NATS server configuration files",
  "version": "0.1.2",
  "engines": { "vscode": "^1.75.0" },
  "categories": ["Programming Languages"],
  "contributes": {
    "languages": [
      {
        "id": "nats-server-conf",
        "aliases": ["NATS Server Config", "nats-server-conf"],
        "extensions": [".conf"],
        "filenames": ["nats-server.conf", "nats.conf"],
        "configuration": "./language-configuration.json"
      }
    ],
    "grammars": [
      {
        "language": "nats-server-conf",
        "scopeName": "source.nats-server-conf",
        "path": "./syntaxes/nats-server-conf.tmLanguage.json"
      }
    ]
  }
}
```

### 3. Create TextMate grammar

Create `syntaxes/nats-server-conf.tmLanguage.json`:

```json
{
  "scopeName": "source.nats-server-conf",
  "name": "NATS Server Configuration",
  "patterns": [
    { "include": "#comment" },
    { "include": "#include" },
    { "include": "#block" },
    { "include": "#pair" },
    { "include": "#value" }
  ],
  "repository": {
    "comment": {
      "patterns": [
        {
          "match": "#.*$",
          "name": "comment.line.hash.nats-server-conf"
        },
        {
          "match": "//.*$",
          "name": "comment.line.double-slash.nats-server-conf"
        }
      ]
    },
    "include": {
      "match": "^\\s*(include)\\s+(.+)$",
      "captures": {
        "1": { "name": "keyword.control.include.nats-server-conf" },
        "2": { "name": "string.unquoted.path.nats-server-conf" }
      }
    },
    "string": {
      "match": "\"(?:[^\"\\\\]|\\\\.)*\"",
      "name": "string.quoted.double.nats-server-conf"
    },
    "number": {
      "match": "-?[0-9]+\\.?[0-9]*(?:[kKmMgGtT][bB]?|[nNuUmM][sS]|µs|[sShHdD])?\\b",
      "name": "constant.numeric.nats-server-conf"
    },
    "boolean": {
      "match": "\\b(?i:true|false|yes|no|on|off)\\b",
      "name": "constant.language.boolean.nats-server-conf"
    },
    "variable": {
      "match": "\\$[a-zA-Z_][a-zA-Z0-9_]*",
      "name": "variable.other.nats-server-conf"
    },
    "key": {
      "match": "^\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=[=:])",
      "captures": {
        "1": { "name": "entity.name.tag.nats-server-conf" }
      }
    },
    "block-name": {
      "match": "^\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=[={])",
      "captures": {
        "1": { "name": "entity.name.type.nats-server-conf" }
      }
    },
    "pair": {
      "patterns": [
        { "include": "#key" },
        { "include": "#value" }
      ]
    },
    "block": {
      "begin": "\\{",
      "end": "\\}",
      "beginCaptures": {
        "0": { "name": "punctuation.definition.block.begin.nats-server-conf" }
      },
      "endCaptures": {
        "0": { "name": "punctuation.definition.block.end.nats-server-conf" }
      },
      "patterns": [
        { "include": "#comment" },
        { "include": "#include" },
        { "include": "#block" },
        { "include": "#pair" },
        { "include": "#value" }
      ]
    },
    "value": {
      "patterns": [
        { "include": "#string" },
        { "include": "#number" },
        { "include": "#boolean" },
        { "include": "#variable" }
      ]
    }
  }
}
```

### 4. Create language configuration

Create `language-configuration.json`:

```json
{
  "comments": {
    "lineComment": "#"
  },
  "brackets": [
    ["{", "}"],
    ["[", "]"]
  ],
  "autoClosingPairs": [
    { "open": "{", "close": "}" },
    { "open": "[", "close": "]" },
    { "open": "\"", "close": "\"" }
  ],
  "surroundingPairs": [
    { "open": "{", "close": "}" },
    { "open": "[", "close": "]" },
    { "open": "\"", "close": "\"" }
  ],
  "folding": {
    "markers": {
      "start": "\\{",
      "end": "\\}"
    }
  }
}
```

### 5. Install the extension

```bash
# Package and install
npx vsce package
code --install-extension vscode-nats-server-conf-0.1.2.vsix
```

Or for development, symlink into the extensions directory:

```bash
ln -s "$(pwd)" ~/.vscode/extensions/vscode-nats-server-conf
```

## Troubleshooting

- **No highlighting**: Check that the file is detected as `nats-server-conf` (see the language indicator in the VS Code status bar)
- **Wrong language detected**: `.conf` is a generic extension. You may need to manually set the language via the status bar or add specific file associations in settings:
  ```json
  {
    "files.associations": {
      "**/nats*.conf": "nats-server-conf",
      "**/nats-server*.conf": "nats-server-conf"
    }
  }
  ```
