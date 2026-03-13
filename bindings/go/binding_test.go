package tree_sitter_nats_server_conf_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_nats_server_conf "github.com/philpennock/treesitter-nats-server/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_nats_server_conf.Language())
	if language == nil {
		t.Errorf("Error loading NatsServerConf grammar")
	}
}
