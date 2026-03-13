import XCTest
import SwiftTreeSitter
import TreeSitterNatsServerConf

final class TreeSitterNatsServerConfTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_nats_server_conf())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading NatsServerConf grammar")
    }
}
