; Local variable scoping for NATS server configuration files

; The source file is the root scope
(source_file) @local.scope

; Key-value pair at root level can define a variable
(source_file
  (pair
    key: (identifier) @local.definition))

; Variable references
(variable_reference) @local.reference
