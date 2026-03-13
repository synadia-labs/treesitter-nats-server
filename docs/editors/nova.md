# Nova Integration

This guide explains how to add NATS server configuration file support to [Panic Nova](https://nova.app/).

Nova supports tree-sitter grammars through its extension system. You can create a Nova extension that bundles the NATS server configuration grammar for syntax highlighting, code folding, and indentation.

## Prerequisites

- Nova 10+ (with tree-sitter support)
- A C compiler (clang, included with Xcode Command Line Tools)
- Node.js (for building the WASM grammar)

## Setup

### 1. Create the extension scaffold

```bash
mkdir -p ~/Library/Application\ Support/Nova/Extensions/nats-server-conf.novaextension
cd ~/Library/Application\ Support/Nova/Extensions/nats-server-conf.novaextension
```

### 2. Create `extension.json`

```json
{
  "identifier": "com.example.nats-server-conf",
  "name": "NATS Server Configuration",
  "organization": "Your Name",
  "description": "Syntax highlighting for NATS server configuration files using tree-sitter.",
  "version": "0.1.1",
  "categories": ["languages"],
  "main": "main.js",
  "activationEvents": [
    "onLanguage:nats-server-conf"
  ]
}
```

### 3. Create the syntax definition

Create `Syntaxes/nats-server-conf.xml`:

```bash
mkdir -p Syntaxes
```

```xml
<?xml version="1.0" encoding="UTF-8"?>
<syntax name="nats-server-conf" xmlns="https://www.nova.app/syntax">
    <meta>
        <name>NATS Server Config</name>
        <type>structured</type>
        <preferred-file-extension>conf</preferred-file-extension>
    </meta>

    <detectors>
        <filename-pattern>*nats*.conf</filename-pattern>
        <filename-pattern>*nats-server*.conf</filename-pattern>
    </detectors>

    <comments>
        <single>
            <starts-with>
                <expression>#</expression>
            </starts-with>
        </single>
        <single>
            <starts-with>
                <expression>//</expression>
            </starts-with>
        </single>
    </comments>

    <brackets>
        <pair open="{" close="}" />
        <pair open="[" close="]" />
    </brackets>

    <surrounding-pairs>
        <pair open="{" close="}" />
        <pair open="[" close="]" />
        <pair open="&quot;" close="&quot;" />
    </surrounding-pairs>

    <scopes>
        <include syntax="self" collection="comments" />
        <include syntax="self" collection="keywords" />
        <include syntax="self" collection="blocks" />
        <include syntax="self" collection="values" />
    </scopes>

    <collections>
        <collection name="comments">
            <scope name="nats-server-conf.comment.single.hash">
                <expression>#(.*)$</expression>
                <capture number="0" name="comment.single" />
            </scope>
            <scope name="nats-server-conf.comment.single.double-slash">
                <expression>//(.*)$</expression>
                <capture number="0" name="comment.single" />
            </scope>
        </collection>

        <collection name="keywords">
            <scope name="nats-server-conf.keyword.include">
                <expression>\b(include)\s+(.+)$</expression>
                <capture number="1" name="keyword" />
                <capture number="2" name="string" />
            </scope>
        </collection>

        <collection name="blocks">
            <scope name="nats-server-conf.block">
                <symbol type="block">
                    <context behavior="subtree" />
                </symbol>
                <starts-with>
                    <expression>([a-zA-Z_][a-zA-Z0-9_]*)\s*(\{)</expression>
                    <capture number="1" name="identifier.type" />
                    <capture number="2" name="bracket" />
                </starts-with>
                <ends-with>
                    <expression>(\})</expression>
                    <capture number="1" name="bracket" />
                </ends-with>
                <subscopes>
                    <include syntax="self" />
                </subscopes>
            </scope>
        </collection>

        <collection name="values">
            <scope name="nats-server-conf.string.double-quoted">
                <expression>"(?:[^"\\]|\\.)*"</expression>
                <capture number="0" name="string" />
            </scope>
            <scope name="nats-server-conf.value.number">
                <expression>-?[0-9]+\.?[0-9]*(?:[kKmMgGtT][bB]?|[nNuUmM][sS]|µs|[sShHdD])?\b</expression>
                <capture number="0" name="value.number" />
            </scope>
            <scope name="nats-server-conf.value.boolean">
                <expression>\b(?i:true|false|yes|no|on|off)\b</expression>
                <capture number="0" name="value.boolean" />
            </scope>
            <scope name="nats-server-conf.identifier.variable">
                <expression>\$[a-zA-Z_][a-zA-Z0-9_]*</expression>
                <capture number="0" name="identifier.variable" />
            </scope>
            <scope name="nats-server-conf.identifier.key">
                <expression>([a-zA-Z_][a-zA-Z0-9_]*)\s*(?=[=:])</expression>
                <capture number="1" name="identifier.property" />
            </scope>
        </collection>
    </collections>
</syntax>
```

### 4. Add a completions file (optional)

Create `Completions/nats-server-conf.xml` for common NATS server configuration keys:

```bash
mkdir -p Completions
```

```xml
<?xml version="1.0" encoding="UTF-8"?>
<completions xmlns="https://www.nova.app/completions">
    <provider name="nats-server-conf.keywords">
        <syntax>nats-server-conf</syntax>
        <set>nats.keywords</set>
    </provider>

    <set name="nats.keywords">
        <completion string="listen" />
        <completion string="port" />
        <completion string="host" />
        <completion string="cluster" />
        <completion string="jetstream" />
        <completion string="tls" />
        <completion string="authorization" />
        <completion string="accounts" />
        <completion string="leafnodes" />
        <completion string="gateway" />
        <completion string="websocket" />
        <completion string="mqtt" />
        <completion string="include" />
    </set>
</completions>
```

### 5. Verify

1. Restart Nova or reload extensions via **Extensions → Extension Library**
2. Open a file matching `*nats*.conf`
3. You should see syntax highlighting and the language shown as "NATS Server Config" in the editor toolbar

## Troubleshooting

- **No highlighting**: Ensure the `.novaextension` directory is in `~/Library/Application Support/Nova/Extensions/` and that the syntax XML is well-formed
- **Extension not appearing**: Check the Nova extension console (**Extensions → Show Extension Console**) for errors
- **Wrong language detected**: `.conf` is a generic extension. Verify that the `<detectors>` patterns match your file name
