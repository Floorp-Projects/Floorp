# `CodePointTrie`

This module provides a data structure for an time-efficient lookup of values
associated to code points.

It is an implementation of the existing [ICU4C UCPTrie](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/ucptrie_8h.html)
/ [ICU4J CodePointTrie](https://unicode-org.github.io/icu-docs/apidoc/dev/icu4j/) API.

## Architecture

ICU4X [`CodePointTrie`](crate::codepointtrie::CodePointTrie) is designed to provide a read-only view of CodePointTrie data that is exported
from ICU4C. Detailed information about the design of the data structure can be found in the documentation
for the [`CodePointTrie`](crate::codepointtrie::CodePointTrie) struct.

## Examples

### Querying a `CodePointTrie`

```rust
use icu_collections::codepointtrie::planes;
let trie = planes::get_planes_trie();

assert_eq!(0, trie.get(0x41)); // 'A' as u32
assert_eq!(0, trie.get(0x13E0)); // 'Ꮰ' as u32
assert_eq!(1, trie.get(0x10044)); // '𐁄' as u32
```

[`ICU4X`]: ../icu/index.html

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
