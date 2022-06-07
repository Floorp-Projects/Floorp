[![Build Status](https://travis-ci.org/phaazon/glsl.svg?branch=master)](https://travis-ci.org/phaazon/glsl)
[![crates.io](https://img.shields.io/crates/v/glsl.svg)](https://crates.io/crates/glsl)
[![docs.rs](https://docs.rs/glsl/badge.svg)](https://docs.rs/glsl)
![License](https://img.shields.io/badge/license-BSD3-blue.svg?style=flat)

<!-- cargo-sync-readme start -->

This crate is a GLSL450/GLSL460 compiler. It’s able to parse valid GLSL formatted source into
an abstract syntax tree (AST). That AST can then be transformed into SPIR-V, your own format or
even folded back to a raw GLSL [`String`] (think of a minifier, for instance).

You’ll find several modules:

  - [`parser`], which exports the parsing interface. This is the place you will get most
    interesting types and traits, such as [`Parse`] and [`ParseError`].
  - [`syntax`], which exports the AST and language definitions. If you look into destructuring,
    transpiling or getting information on the GLSL code that got parsed, you will likely
    manipulate objects which types are defined in this module.
  - [`transpiler`], which provides you with GLSL transpilers. For instance, you will find _GLSL
    to GLSL_ transpiler, _GLSL to SPIR-V_ transpiler, etc.
  - [`visitor`](visitor), which gives you a way to visit AST nodes and mutate them, both with
    inner and outer mutation.

Feel free to inspect those modules for further information.

# GLSL parsing and transpiling

Parsing is the most common operation you will do. It is not required per-se (you can still
create your AST by hand or use [glsl-quasiquote] to create it at compile-time by using the GLSL
syntax directly in Rust). However, in this section, we are going to see how we can parse from a
string to several GLSL types.

## Parsing architecture

Basically, the [`Parse`] trait gives you all you need to start parsing. This crate is designed
around the concept of type-driven parsing: parsers are hidden and you just have to state what
result type you expect.

The most common type you want to parse to is [`TranslationUnit`], which represents a set of
[`ExternalDeclaration`]s. An [`ExternalDeclaration`] is just a declaration at the top-most level
of a shader. It can be a global, uniform declarations, vertex attributes, a function, a
structure, etc. In that sense, a [`TranslationUnit`] is akin to a shader stage (vertex shader,
fragment shader, etc.).

You can parse any type that implements [`Parse`]. Parsers are mostly sensible to external
blanks, which means that parsing an [`Expr`] starting with a blank will not work (this is not
true for a [`TranslationUnit`] as it’s exceptionnally more permissive).

## Parsing an expression

Let’s try to parse an expression.

```rust
use glsl::parser::Parse as _;
use glsl::syntax::Expr;

let glsl = "(vec3(r, g, b) * cos(t * PI * .5)).xxz";
let expr = Expr::parse(glsl);
assert!(expr.is_ok());
```

Here, `expr` is an AST which type is `Result<Expr, ParseError>` that represents the GLSL
expression  `(vec3(r, g, b) * cos(t * PI * .5)).xxz`, which is an outer (scalar) multiplication
of an RGB color by a cosine of a time, the whole thing being
[swizzled](https://en.wikipedia.org/wiki/Swizzling_\(computer_graphics\)) with XXZ. It is your
responsibility to check if the parsing process has succeeded.

In the previous example, the GLSL string is a constant and hardcoded. It could come from a file,
network or built on the fly, but in the case of constant GLSL code, it would be preferable not
to parse the string at runtime, right? Well, [glsl-quasiquote] is there exactly for that. You
can ask **rustc** to parse that string and, if the parsing has succeeded, inject the AST
directly into your code. No [`Result`], just the pure AST. Have a look at [glsl-quasiquote] for
further details.

## Parsing a whole shader

Vertex shaders, geometry shaders, fragment shaders and control and evaluation tessellation
shaders can be parsed the same way by using one of the `TranslationUnit` or `ShaderStage` types.

Here, a simple vertex shader being parsed.

```rust
use glsl::parser::Parse as _;
use glsl::syntax::ShaderStage;

let glsl = "
  layout (location = 0) in vec3 pos;
  layout (location = 1) in vec4 col;

  out vec4 v_col;

  uniform mat4 projview;

  void main() {
    v_col = col; // pass color to the next stage
    gl_Position = projview * vec4(pos, 1.);
  }
";
let stage = ShaderStage::parse(glsl);
assert!(stage.is_ok());
```

## Visiting AST nodes

The crate is also getting more and more combinators and functions to transform the AST or create
nodes with regular Rust. The [`Visitor`] trait will be a great friend of yours when you will
want to cope with deep mutation, filtering and validation. Have a look at the
[`visitor`](visitor) module for a tutorial on how to use visitors.

# About the GLSL versions…

This crate can parse both GLSL450 and GLSL460 formatted input sources. At the language level,
the difference between GLSL450 and GLSL460 is pretty much nothing, so both cases are covered.

> If you’re wondering, the only difference between both versions is that in GLSL460, it’s
> authorized to have semicolons (`;`) on empty lines at top-level in a shader.

[glsl-quasiquote]: https://crates.io/crates/glsl-quasiquote
[`Parse`]: crate::parser::Parse
[`ParseError`]: crate::parser::ParseError
[`ExternalDeclaration`]: crate::syntax::ExternalDeclaration
[`TranslationUnit`]: crate::syntax::TranslationUnit
[`Expr`]: crate::syntax::Expr
[`Visitor`]: crate::visitor::Visitor

<!-- cargo-sync-readme end -->
