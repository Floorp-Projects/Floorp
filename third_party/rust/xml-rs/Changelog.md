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
