# webidl-rs

[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build Status](https://travis-ci.org/sgodwincs/webidl-rs.svg?branch=master)](https://travis-ci.org/sgodwincs/webidl-rs)
[![Crates.io Version](https://img.shields.io/crates/v/webidl.svg)](https://crates.io/crates/webidl)

A parser for [WebIDL](https://heycam.github.io/webidl/) in Rust.

[Documentation](https://docs.rs/webidl/)

# Example

## Lexing

```rust
use webidl::*;

let lexer = Lexer::new("/* Example taken from emscripten site */\n\
                        enum EnumClass_EnumWithinClass {\n\
                            \"EnumClass::e_val\"\n\
                        };");
assert_eq!(lexer.collect::<Vec<_>>(),
           vec![Ok((41, Token::Enum, 45)),
                Ok((46, Token::Identifier("EnumClass_EnumWithinClass".to_string()), 71)),
                Ok((72, Token::LeftBrace, 73)),
                Ok((74, Token::StringLiteral("EnumClass::e_val".to_string()), 92)),
                Ok((93, Token::RightBrace, 94)),
                Ok((94, Token::Semicolon, 95))]);
```

## Parsing

```rust
use webidl::*;
use webidl::ast::*;

let result = parse_string("[Attribute] interface Node { };");

assert_eq!(result,
           Ok(vec![Definition::Interface(Interface::NonPartial(NonPartialInterface {
                extended_attributes: vec![
                    Box::new(ExtendedAttribute::NoArguments(
                        Other::Identifier("Attribute".to_string())))],
                inherits: None,
                members: vec![],
                name: "Node".to_string()
           }))]));
```

## Pretty printing AST

An example of a visitor implementation can be found [here](https://github.com/sgodwincs/webidl-rs/blob/master/src/parser/visitor/pretty_print.rs). Below is an example of how it is used:

```rust
use webidl::ast::*;
use webidl::visitor::*;

let ast = vec![Definition::Interface(Interface::NonPartial(NonPartialInterface {
                extended_attributes: vec![
                    Box::new(ExtendedAttribute::NoArguments(
                        Other::Identifier("Attribute".to_string())))],
                inherits: None,
                members: vec![InterfaceMember::Attribute(Attribute::Regular(RegularAttribute {
                             extended_attributes: vec![],
                             inherits: false,
                             name: "attr".to_string(),
                             read_only: true,
                             type_: Box::new(Type {
                                 extended_attributes: vec![],
                                 kind: TypeKind::SignedLong,
                                 nullable: true
                             })
                         }))],
                name: "Node".to_string()
          }))];
let mut visitor = PrettyPrintVisitor::new();
visitor.visit(&ast);
assert_eq!(visitor.get_output(),
           "[Attribute]\ninterface Node {\n    readonly attribute long? attr;\n};\n\n");
```

# Conformance

The parser is conformant with regards to the [WebIDL grammar](https://heycam.github.io/webidl/#idl-grammar) except for three points:

- Extended attributes, as described by the grammar, are not supported due to their lack of semantic meaning when parsed. Instead, limited forms are supported (as shown in the [table](https://heycam.github.io/webidl/#idl-extended-attributes)). This parser allows a bit more flexibility when parsing extended attributes of the form `A=B`. The specification states that `A` and `B` must be identifiers, but this parser accepts `B` as any token. If you would like for any extended attributes to be parsed (essentially any sequences of tokens), please consider looking at [#8](https://github.com/sgodwincs/webidl-rs/issues/8) to help resolve the problem with doing so.
- This parser supports the old `implements` keyword that is no longer a part of the official specification. This is for backwards compatibility. 
- This parser supports the old `legacycaller` keyword that is no longer a part of the official specification. This is for backwards compatibility. 
