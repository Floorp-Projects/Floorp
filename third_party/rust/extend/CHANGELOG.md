# Change Log

All user visible changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/), as described
for Rust libraries in [RFC #1105](https://github.com/rust-lang/rfcs/blob/master/text/1105-api-evolution.md)

## Unreleased

None.

### Breaking changes

None.

## 1.1.2 - 2021-09-02

- Fix using `pub impl` with `#[async_trait]`.

## 1.1.1 - 2021-06-12

- Fix name collision for extensions on `&T` and `&mut T`. The generated traits
  now get different names.

## 1.1.0 - 2021-06-12

- Support setting visibility of the generated trait directly on the `impl`
  block. For example: `pub impl i32 { ... }`.
- Add `#[ext_sized]` for adding `Sized` supertrait.

## 1.0.1 - 2021-02-14

- Update maintenance status.

## 1.0.0 - 2021-01-30

- Support extensions on bare functions types (things like `fn(i32) -> bool`).
- Support extensions on trait objects (things like `dyn Send + Sync`).

## 0.3.0 - 2020-08-31

- Add async-trait compatibility.

### Breaking changes

- Other attributes put on the `impl` would previously only be included on the generated trait. They're now included on both the trait and the implementation.

## 0.2.1 - 2020-08-29

- Fix documentation link in Cargo.toml.
- Use more correct repository URL in Cargo.toml.

## 0.2.0 - 2020-08-29

- Handle unnamed extensions on the same generic type with different type parameters. For example `Option<i32>` and `Option<String>`. Previously we would generate the same name of both hidden traits which wouldn't compile.
- Support associated constants in extension impls.

### Breaking changes

- Generated traits are no longer sealed and the `sealed` argument previously supported by `#[ext]` has been removed. Making the traits sealed lead to lots of complexity that we didn't think brought much value.

## 0.1.1 - 2020-02-22

- Add support for specifying supertraits of the generated trait [#4](https://github.com/davidpdrsn/extend/pull/4).

## 0.1.0

- Support adding extensions to the ["never type"](https://doc.rust-lang.org/std/primitive.never.html).

### Breaking changes

- Simplify names of traits generates for complex types.

## 0.0.2

- Move "trybuild" to dev-dependency.

## 0.0.1

Initial release.
