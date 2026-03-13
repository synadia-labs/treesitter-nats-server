// swift-tools-version:5.3

import Foundation
import PackageDescription

var sources = ["src/parser.c"]
if FileManager.default.fileExists(atPath: "src/scanner.c") {
    sources.append("src/scanner.c")
}

let package = Package(
    name: "TreeSitterNatsServerConf",
    products: [
        .library(name: "TreeSitterNatsServerConf", targets: ["TreeSitterNatsServerConf"]),
    ],
    dependencies: [
        .package(name: "SwiftTreeSitter", url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.9.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterNatsServerConf",
            dependencies: [],
            path: ".",
            sources: sources,
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterNatsServerConfTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterNatsServerConf",
            ],
            path: "bindings/swift/TreeSitterNatsServerConfTests"
        )
    ],
    cLanguageStandard: .c11
)
