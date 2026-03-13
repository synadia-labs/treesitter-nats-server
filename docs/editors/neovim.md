# Neovim Integration

This guide explains how to add NATS server configuration file support to Neovim using [nvim-treesitter](https://github.com/nvim-treesitter/nvim-treesitter).

## Prerequisites

- Neovim 0.9+ with tree-sitter support
- [nvim-treesitter](https://github.com/nvim-treesitter/nvim-treesitter) plugin installed
- A C compiler (gcc or clang)

## Setup

### 1. Register the parser

Add this to your Neovim configuration (e.g., `init.lua`):

```lua
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()

parser_config.nats_server_conf = {
  install_info = {
    url = "https://github.com/synadia-labs/treesitter-nats-server",
    files = { "src/parser.c", "src/scanner.c" },
    branch = "main",
    generate_requires_npm = false,
    requires_generate_from_grammar = false,
  },
  filetype = "nats-server-conf",
}
```

### 2. Set up filetype detection

Create `~/.config/nvim/ftdetect/nats.lua`:

```lua
vim.filetype.add({
  pattern = {
    -- Files specifically named nats*.conf or nats-server*.conf (or with underscore)
    [".*nats[%-_]?server.*%.conf"] = "nats-server-conf",
    [".*nats%.conf"] = "nats-server-conf",

    -- Any .conf file under a directory whose name contains "nats"
    [".*/[^/]*nats[^/]*/.*%.conf"] = "nats-server-conf",
  },
})
```

Or if you prefer Vimscript, create `~/.config/nvim/ftdetect/nats.vim`:

```vim
autocmd BufRead,BufNewFile *nats-server*.conf,*nats*.conf set filetype=nats-server-conf
```

### 3. Install the parser

In Neovim, run:

```vim
:TSInstall nats_server_conf
```

### 4. Install query files

Copy the query files from this repository into your Neovim runtime:

```bash
QUERIES_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/nvim/queries/nats_server_conf"
mkdir -p "$QUERIES_DIR"
cp queries/highlights.scm "$QUERIES_DIR/"
cp queries/folds.scm "$QUERIES_DIR/"
cp queries/indents.scm "$QUERIES_DIR/"
cp queries/locals.scm "$QUERIES_DIR/"
```

### 5. Verify

Open a `.conf` file and run:

```vim
:InspectTree
```

You should see the parsed syntax tree. Syntax highlighting should also be active.

## lazy.nvim Example

If you use [lazy.nvim](https://github.com/folke/lazy.nvim):

```lua
{
  "nvim-treesitter/nvim-treesitter",
  build = ":TSUpdate",
  config = function()
    local parser_config = require("nvim-treesitter.parsers").get_parser_configs()

    parser_config.nats_server_conf = {
      install_info = {
        url = "https://github.com/synadia-labs/treesitter-nats-server",
        files = { "src/parser.c", "src/scanner.c" },
        branch = "main",
      },
      filetype = "nats-server-conf",
    }

    require("nvim-treesitter.configs").setup({
      ensure_installed = { "nats_server_conf" },
      highlight = { enable = true },
      indent = { enable = true },
    })
  end,
}
```

### 6. Additional optional improvements

Let the filetype system know about the existence of `nats-server-conf` as a
filetype, so that you can type `:set ft=nats<tab>` and have the type work:

```sh
touch "${XDG_CONFIG_HOME:-$HOME/.config}/nvim/ftplugin/nats-server-conf.lua"
```

This may help some components integrate fully, by associating the filetype
name with hyphens with the treesitter name with underscores:

```lua
vim.treesitter.language.register('nats_server_conf', 'nats-server-conf')
```


## Troubleshooting

- **No highlighting**: Ensure query files are in the correct directory and the filetype is detected (``:set ft?``)
- **Parser not found**: Run `:TSInstallInfo` and check that `nats_server_conf` appears in the list
- **Compilation errors**: Ensure you have a C compiler available and that `cc` or `gcc` is in your `PATH`
