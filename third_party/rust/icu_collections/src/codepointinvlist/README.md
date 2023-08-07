# `CodePointInversionList`

This module provides necessary functionality for highly efficient querying of sets of Unicode characters.

It is an implementation of the existing [ICU4C UnicodeSet API](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/classicu_1_1UnicodeSet.html).

## Architecture
ICU4X [`CodePointInversionList`] is split up into independent levels, with [`CodePointInversionList`] representing the membership/query API,
and [`CodePointInversionListBuilder`] representing the builder API.

## Examples:

### Creating a `CodePointInversionList`

CodePointSets are created from either serialized [`CodePointSets`](CodePointInversionList),
represented by [inversion lists](http://userguide.icu-project.org/strings/properties),
the [`CodePointInversionListBuilder`], or from the Properties API.

```rust
use icu_collections::codepointinvlist::{CodePointInversionList, CodePointInversionListBuilder};

let mut builder = CodePointInversionListBuilder::new();
builder.add_range(&('A'..'Z'));
let set: CodePointInversionList = builder.build();

assert!(set.contains('A'));
```

### Querying a `CodePointInversionList`

Currently, you can check if a character/range of characters exists in the [`CodePointInversionList`], or iterate through the characters.

```rust
use icu_collections::codepointinvlist::{CodePointInversionList, CodePointInversionListBuilder};

let mut builder = CodePointInversionListBuilder::new();
builder.add_range(&('A'..'Z'));
let set: CodePointInversionList = builder.build();

assert!(set.contains('A'));
assert!(set.contains_range(&('A'..='C')));
assert_eq!(set.iter_chars().next(), Some('A'));
```

[`ICU4X`]: ../icu/index.html

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
