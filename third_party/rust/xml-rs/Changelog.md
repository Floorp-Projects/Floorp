## Version 0.8.3

* Added a new parser option, `ignore_root_level_whitespace`, which makes the parser
  skip emitting whitespace events outside of the root element when set to `true`.
  This helps with certain tasks like canonicalization.

## Version 0.8.2

* Added a new parser option, `replace_unknown_entity_references`, which allows to ignore
  invalid Unicode code points and replace them with a Unicode "replacement character"
  during parsing. This can be helpful to deal with e.g. UTF-16 surrogate pairs.
* Added a new emitter option, `pad_self_closing`, which determines the style of the self-closing
  elements when they are emitted: `<a />` (`true`) vs `<a/>` (`false`).

## Version 0.8.1

* Fixed various issues with tests introduced by updates in Rust.
* Adjusted the lexer to ignore contents of the `<!DOCTYPE>` tag.
* Removed unnecessary unsafety in tests.
* Added tests for doc comments in the readme file.
* Switched to GitHub Actions from Travis CI.

## Version 0.8.0

* Same as 0.7.1, with 0.7.1 being yanked because of the incorrect semver bump.

## Version 0.7.1

* Removed dependency on bitflags.
* Added the `XmlWriter::inner_mut()` method.
* Fixed some rustdoc warnings.

## Version 0.7.0

* Same as 0.6.2, with 0.6.2 being yanked because of the incompatible bump of minimum required version of rustc.

## Version 0.6.2

* Bumped `bitflags` to 1.0.

## Version 0.6.1

* Fixed the writer to escape some special characters when writing attribute values.

## Version 0.6.0

* Changed the target type of extra entities from `char` to `String`. This is an incompatible
  change.

## Version 0.5.0

* Added support for ignoring EOF errors in order to read documents from streams incrementally.
* Bumped `bitflags` to 0.9.

## Version 0.4.1

* Added missing `Debug` implementation to `xml::writer::XmlEvent`.

## Version 0.4.0

* Bumped version number, since changes introduced in 0.3.7 break backwards compatibility.

## Version 0.3.8

* Fixed a problem introduced in 0.3.7 with entities in attributes causing parsing errors.

## Version 0.3.7

* Fixed the problem with parsing non-whitespace character entities as whitespace (issue #140).
* Added support for configuring custom entities in the parser configuration.

## Version 0.3.6

* Added an `Error` implementation for `EmitterError`.
* Fixed escaping of strings with multi-byte code points.

## Version 0.3.5

* Added `Debug` implementation for `XmlVersion`.
* Fixed some failing tests.

## Version 0.3.3

* Updated `bitflags` to 0.7.

## Version 0.3.2

* Added `From<io::Error>` for `xml::reader::Error`, which improves usability of working with parsing errors.

## Version 0.3.1

* Bumped `bitflags` dependency to 0.4, some internal warning fixes.

## Version 0.3.0

* Changed error handling in `EventReader` - now I/O errors are properly bubbled up from the lexer.

## Version 0.2.4

* Fixed #112 - incorrect handling of namespace redefinitions when writing a document.

## Version 0.2.3

* Added `into_inner()` methods to `EventReader` and `EventWriter`.

## Version 0.2.2

* Using `join` instead of the deprecated `connect`.
* Added a simple XML analyzer program which demonstrates library usage and can be used to check XML documents for well-formedness.
* Fixed incorrect handling of unqualified attribute names (#107).
* Added this changelog.

## Version 0.2.1

* Fixed #105 - incorrect handling of double dashes.

## Version 0.2.0

* Major update, includes proper document writing support and significant architecture changes.
