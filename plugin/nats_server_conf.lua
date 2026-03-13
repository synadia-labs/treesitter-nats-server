-- Register language aliases so that markdown code fences with alternative
-- names (e.g. ```nats-server-conf or ```nats-conf) trigger this parser.
-- This file is auto-sourced by Neovim when this repo is loaded as a plugin.
if vim.treesitter and vim.treesitter.language and vim.treesitter.language.register then
  vim.treesitter.language.register('nats_server_conf', {
    'nats-server-conf',
    'nats-conf',
    'nats_conf',
  })
end
