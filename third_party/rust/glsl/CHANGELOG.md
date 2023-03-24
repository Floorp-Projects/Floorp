# Changelog

# 6.0.2

> Mar 23, 2023

- Switch to `nom-7`.

# 6.0.1

> Jul 11, 2021

- Better scheme for parenthesis generation in the GLSL transpiler.
- Fix matrices types in the GLSL transpiler.
- Fix end-of-line parser, which now accepts CR LF too.

# 6.0

> Dec 7th, 2020

- Change the meaning of `Visitor` and `Host`. They now take the AST nodes by simple references (`&`) and not via
  mutable references (`&mut`) anymore. This will allow people to use visitors in much more contexts.
- Add `VisitorMut` and `HostMut` to visit AST nodes mutably. These correspond to the previous (version `<6.0`) `Visitor`
  and `Host` traits. If you were using them and require mutability, feel free to simply switch to `VisitorMut` and
  `HostMut`.

# 5.0.2

> Nov 1st, 2020

- Bump `nom-6.0`.

# 5.0.1

> Aug 12th, 2020

- Fix float / double literal parsing priority. Floats are parsed first now in case of a polymorphic
  constant.

# 5.0.0

> Jul 27th, 2020

- Fix array specifiers by splitting the `ArraySpecifier` type into two types:
  - `ArraySpecifier`, which holds a non-empty list of `ArraySpecifierDimension`.
  - `ArraySpecifierDimension`, which is the “old” `ArraySpecifier`.
  This change allows for multidimensional array specifiers.

## Migration guide

- If you were using array specifiers, you need to wrap them inside an `ArraySpecifierDimension`
  and wrap it in a `ArraySpecifier { dimensions: NonEmpty(vec![here]) }` expression, where `here`
  is your specifier.

# 4.1.1

> Wed Jul 1st 2020

- Fix the _identifier_ parser, that previously failed to parse identifiers starting with an
  underscore (`_`) while it is permitted (chapter 3.7 of the GLSLang spec).
- Fix associativity for logical binary operators (`||`, `&&`).

# 4.1

> Fri Jun 19th 2020

- Implement `std::error::Error` for error types.

# 4.0.3

> Fri Mar 6th 2020

- Move all the unit tests code into their ow module to prevent rustc from parsing them if we
  don’t build with tests.

# 4.0.2

> Mon 10th Feb 2020

- Remove the `lexical` feature from `nom`. It’s useful only when using functions such as the
  `float` combinator, which we don’t use.

# 4.0.1

> Tue 21st Jan 2020

- Fix a bug occurring in the function-like `#define` parser (i.e. `#define foo(x) (y)`) that
  would prevent the use of whitespaces in the argument list.

# 4.0

> Mon 6th Jan 2020

## Major changes

- Add support for `attribute` and `varying` keyword as a backward-compatible parser.
- Fix binary operator associativity. They were (_erroneously_) right-associative. They’re now
  parsed as left-associative.

# 3.0

> Wed 14th Nov 2019

## Major changes

- `JumpStatement::Return` now takes an optional `Expr` instead of an `Expr` directly. That allows
  for parsing statements such as `return;`, which should have
  been supported from the beginning.
- Support for missing preprocessor directives:
  - `#if`.
  - `#ifdef`.
  - `#ifndef`.
  - `#elseif`.
  - `#else`.
  - `#endif`.
  - `#error`.
  - `#include`.
  - `#line`.
  - `#pragma`.
  - `#undef`.

## Patch changes

- Add a `rustfmt.toml` to reformat everything and stay consistent. If you want contribute, ensure
  you are correctly using `rustfmt` (either by running `cargo fmt` before submitting a PR or by
  configuring your editor to format the code for you). This is important, as not respecting the
  formatting would make your contribution impossible to merge due to a CI check.
- Support for _multiline_ annotations (`\`). Multiline annotations are currently supported as
  part of _spaces_ — i.e. you cannot use them to break an identifier on two lines for instance.
  This choice makes it faster to parse without too much compromises (after all, `\` is mostly used
  in CPP directives in GLSL).
- Fix a bug with _recoverable parsers_ that would produce parent parsers to ignore GLSL grammar
  errors. That fix also implies a boost in performance due to short-circuiting optimizations.

# 2.0.1

> Fri 8th Nov 2019

- Improve performance of expression parsers.

# 2.0

> Thu 24th Oct 2019

## Major changes

- Add `ShaderKind::Compute`.
- Remove `NonEmpty::from_iter` and `TranslationUnit::from_iter` as they were deprecated. Use
  `*::from_non_empty_iter` instead.

## Patch changes

- Fix tokenizer of `Block`.
- Fix a bug while parsing floating-point numbers.
- Reformat with `rustfmt`.

# 1.2

> Wed 18th Sep 2019

## Deprecations

- `NonEmpty::from_iter` and `TranslationUnit::from_iter` are deprecated in favor of
  `*::from_non_empty_iter`.

## Minor changes

- Add binary SPIR-V transpilation. That enables to transpile GLSL directly into a SPIR-V buffer.
- Add `NonEmpty::from_non_empty_iter` and `TranslationUnit::from_non_empty_iter`.

# 1.1.1

> Tue 17th Sep 2019

- Update internal code for Rust edition 2018.

# 1.1

> Tue 30th of July 2019

- Add the `ShaderStage` type alias to `TranslationUnit`.
- Enhance the front documentation to showcase how to use how to use the crate.

# 1.0.2

> Tue 23rd of July 2019

- Change the description of the project and update documentation.

# 1.0.1

> Tue 23rd of July 2019

- Change the `external_declaration` parser so that it can also accept _GLSL460_. That should be a
  breaking change because now, _GLSL450_ formatted input accepts feature from _GLSL460_, which
  shouldn’t be allowed. Nevertheless, the added feature (being able to use semicolons (`;`) on
  empty lines at top-level) is not really an interesting property and no breakage should happen.

# 1.0

> Thu 18th of July 2019

- Migrate all parsers to [nom-5](https://crates.io/crates/nom/5.0.0).
- Improve and add unit and integration tests.
- Improve overall documentation.
- Enhance some allocation scheme (removed them by using more adapted parsers).
- Completely remove the byte (`&[u8]`) parsing. That was a bad idea, for both its impractical
  aspect and error removing. Parsing is done on string slices now (`&str`).

# 0.13.5

> Sun 9th of December 2018

- Add the SPIR-V transpiler. Currently, it’s feature-gated and *very* experimental. Feel free to
  try and provide feedback about it.
- Add simple accessors for `ParseResult`.
- Fix a typo in the documentation of syntax.
- Add some unit and integration tests.
- Add `Identifier::as_str` and `TypeName::as_str`

# 0.13.4

> Wed 25nd of November 2018

- Add `NonEmpty::push` and `NonEmpty::pop`.
- Implement `Deref` and `DerefMut` for `TranslationUnit`.

# 0.13.3

> Wed 24nd of November 2018

- Add `NonEmpty::from_iter` and `TranslationUnit::from_iter`.
- Implement `IntoIterator` for `NonEmpty` and `TranslationUnit`.

# 0.13.2

> Wed 22nd of November 2018

- Fix a typo in documentation.

# 0.13.1

> Wed 22nd of November 2018

- Fix a link in documentation.

# 0.13

> Wed 21st of November 2018

- Update/reset hyperlinks in all the documentation for types, traits, methods, etc.
- General enhancement of the documentation.
- `ExternalDeclaration::new_struct` can now fail. Check the doc for further details.
- `NonEmpty`, `Identifier` and `TypeName` and `TranslationUnit` are now plain types and not
  aliases anymore.
- Add AST visitors. Visitors allow for traversing through an AST and on-the-fly mutation,
  filtering, etc.
- The `#define` preprocessor pragma is now supported in a limited form (it can only be used in
  the global scope).

# 0.12

> Sun 11th of November 2018

- Fix the type of identifier stored in `Block`: it now uses the new encoding for arrayed
  identifiers.
- `Block` parsers update for the arrayd identifier change.

# 0.11

> Sat 10th of November 2018

- Add helper functions to build objects form the `syntax` module. Those are intended to be a
  simple way to build ASTs out of Rust code instead of parsing.
- Enhance the documentation of the `Preprocessor` type.

# 0.10.1

> Fri 2nd of November 2018

- Add some missing implementors of `Parse` for types from `glsl::syntax`.

# 0.10

> Fri 2nd of November 2018

- Hide the `parsers` module. It’s not exposed anymore as it was mostly
  [nom](https://crates.io/crates/nom) parsers and we don’t like leaking internals.
- Introduce the `Parser` trait, acting as an abstraction over the internal parsers. Implementors
  provide a type-driven parsing experience that is very similar to the one as
  [FromStr](https://doc.rust-lang.org/std/str/trait.FromStr.html). This change is actually
  mandatory for the [glsl-quasiquote](https://crates.io/crates/glsl-quasiquote) crate’s `glsl!`
  proc-macro to allow people use it for any GLSL item (and not only `TranslationUnit`).
- Enhance the overall documentation.

# 0.9.2

> Wed 3rd of October 2018

 - Fix GLSL transpiled representation of `IVec*`. It was plain wrong.

# 0.9.1

> Sat 7th of July 2018

- Fix unit testing in transpilers.

# 0.9

> Sat 7th of July 2018

- Big cleanup of the module hierarchy.
- Enhanced the documentation.

# 0.8.1

> Sun, 17th of June 2018

- Add the `README.md` path to the `Cargo.toml` manifest.

# 0.8

> Sun 17th of June 2018

This version introduces breaking changes because public types used in return positions have changed.
These concern only intermediate `nom` functions, so if you do not make a fancy use of this crate,
you souldn’t have to worry too much when migrating.

- Fix the roundtrip issue with the GLSL writer (precedence wasn’t correctly respected).
- Simplify internal code.
- Error instead of panicking when parsing overflowing integer literals.
- Fix panic trying to parse literals starting with whitespace.
- Add fuzzing to find out panics.

# 0.7.2

> Wed 13th of December 2017

- Fix the `show_expr` when the `Expr` is a `Expr::UIntConst`.

# 0.7.1

> Mon 20th of November 2017

- `std::error::Error` is now implemented for `ParseError`.

# 0.7

> Wed 27th of September 2017

- Add support for postfix expressions as function identifiers.

# 0.6.5

> Mon 4th of September 2017

- Fix the formatting of floating values when the fractional part is `0`.

# 0.6.4

> Mon 4th of September 2017

- Fix the output for `show_struct_specifier`.

# 0.6.3

> Mon 4th of September 2017

- Fix the output for `show_struct_specifier`.

# 0.6.2

> Mon 4th of September 2017

- Remove a warning.

# 0.6.1

> Mon 4th of September 2017

- Fix `show_struct_specifier`.

# 0.6

> Fri 1st of September 2017

- The `TypeSpecifier` type was wrong as it didn’t carry any `ArraySpecifier` information while the
  GLSL specification’s grammar about type specifiers states they should. Fixed.

# 0.5

> Mon 7th of August 2017

- The `parse` and `parse_str` functions now take as second argument the parser to run. This enables
  using those functions and all the neat logic the wrap in dependent projects.

# 0.4.2

> Fri 4th of August 2017

- A GLSL writer is now available.
- Some parsers yield non-empty list of syntax trees. Those had the incorrect `Vec` type. They were
  replaced by `NonEmpty`, which is an alias to `Vec`, but carry the semantic that it has at least
  one element in it.

# 0.4.1

> Thu 3rd of August 2017

- Uni/multi-line comments are now supported.

# 0.4

> Wed 2nd of August 2017

- The `Declaration::Block` variant was refactored for a better usage.
- Dot field selections and, in a mory general way, postfix expressions completely fixed. The
  `syntax` module was altered to make it easier to work with dot field selection. Also related,
  the function identifier syntax is now isomorphic to an identifier.

# 0.3.1

> Tue 1st of August 2017

- Fix the `preprocessor` parser so that it eats surrounding blanks.

# 0.3

> Mon 31st of July 2017

- Add a very minimalistic yet working preprocessor. It parses `#version` and `#extension`
  commands. Those have to be declared at the top of your file, even though this implementation
  accepts them at any place an external declaration could be defined. Feel free to submit a PR
  if you want to change that behavior, I don’t really mind.
- Enhance the runtime error reporting. It’s not perfect, but it’s way better than before!
- `void` is now recognized as `TypeSpecifier::Void` instead of the erroneous
  `TypeSpecifier::TypeName("void")`.

# 0.2.2

> Mon 31st of July 2017

- The `layout` parser had a nasty bug that would treat a list of key-value pairs as an expression
  assignment. This was fixed and it now treats it as a list of pairs of identifier associated with a
  possible constant expression.
- The `StructFieldSpecifier` type and its associated parser were wrong. Was missing:
  + the type qualifier
  + for each identifier defined in the field specifier, its optional array specifier, as in
    `float foo[3];` or `vec3 bar[];` for unsized ones.

# 0.2.1

> Sun 30th of July 2017

- More documentation to help people to get their feet wet.

# 0.2

> Sat 29th of July 2017

- The whole parsing API is public.

# 0.1

- Initial revision.
