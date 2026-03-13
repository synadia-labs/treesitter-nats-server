# Agent Instructions

This repository contains a [tree-sitter](https://tree-sitter.github.io/)
grammar for [NATS server](https://nats.io/) configuration files.

## Project overview

The grammar parses `.conf` files used by `nats-server`. Key source files:

- `grammar.js` — the grammar definition (entry point for tree-sitter)
- `src/scanner.c` — external scanner for context-sensitive tokens (bare
  strings, comments, identifiers, booleans)
- `queries/` — highlight, fold, indent, and locals queries (`.scm` files)
- `test/corpus/` — tree-sitter test corpus (`.txt` files)
- `examples/` — sample NATS config files used by `tree-sitter parse --stat`

## Prerequisites

- **Node.js 22.x** (see `.tool-versions`; Node 24+ is incompatible with
  tree-sitter-cli 0.25.x)
- **tree-sitter CLI** — install via `cargo install tree-sitter-cli` (not an
  npm dependency)
- **[Task](https://taskfile.dev/)** (optional but recommended) — task runner;
  see `Taskfile.yaml`

## Common workflows

All commands assume you are in the repository root.

### Setup

```sh
npm ci
command -v tree-sitter || cargo install tree-sitter-cli
```

### Generate the parser

After editing `grammar.js`, regenerate the C parser:

```sh
tree-sitter generate   # or: task generate
```

### Run tests

```sh
tree-sitter test       # or: task test
```

This runs the corpus in `test/corpus/`. Every test file contains input
fragments and expected S-expression parse trees. When adding or changing
grammar rules, update or add corpus tests to cover them.

### Parse example files

```sh
tree-sitter parse examples/*.conf --stat   # or: task test:parse-examples
```

All example files must parse without errors.

### Full CI check (locally)

```sh
task ci:test
```

This runs both `test` and `test:parse-examples`.

## Making changes

### Grammar changes (`grammar.js` / `src/scanner.c`)

1. Edit `grammar.js` (and `src/scanner.c` if the change involves external
   tokens).
2. Run `tree-sitter generate` to regenerate `src/parser.c`.
3. Add or update test cases in `test/corpus/`.
4. Run `tree-sitter test` — all tests must pass.
5. Run `tree-sitter parse examples/*.conf --stat` — no errors allowed.
6. If the change adds or renames node types, update the queries in `queries/`
   to match.

### Query changes (`queries/*.scm`)

- `highlights.scm` — syntax highlighting
- `folds.scm` — code folding regions
- `indents.scm` — auto-indentation rules
- `locals.scm` — scope/reference tracking

After editing queries, run `tree-sitter test` to ensure they are still valid
against the grammar. Highlight queries can be previewed with
`tree-sitter highlight examples/full.conf`.

### Test corpus (`test/corpus/*.txt`)

Tests use tree-sitter's standard format:

```
===
Test name
===
input text
---
(expected_sexpression)
```

Group related tests in the appropriate file (e.g., `blocks.txt` for block
syntax, `types.txt` for value types). Each test should be focused and minimal.

### Editor integration docs (`docs/editors/`)

These are per-editor setup guides. When changing grammar node names or query
file structure, review and update affected editor docs.

## CI

The GitHub Actions workflow (`.github/workflows/ci.yaml`) runs on pushes to
`main` and on pull requests. It runs two jobs:

- **Test** — `task ci:test` (generate, test, parse examples)
- **Check GitHub Actions** — lints workflows with
  [zizmor](https://github.com/woodruffw/zizmor)

All CI checks must pass before merging.

## Code style

- `grammar.js` uses the tree-sitter DSL (`grammar()`, `seq()`, `choice()`,
  `repeat()`, etc.). Keep rules concise and well-commented.
- The external scanner (`src/scanner.c`) is plain C. Follow the existing style
  and keep functions short.
- Query files (`.scm`) use S-expression syntax. One capture pattern per line.
- Do not edit `src/parser.c` by hand — it is generated.
- Do not commit `node_modules/`, build artifacts, or `.wasm` files (see
  `.gitignore`).
