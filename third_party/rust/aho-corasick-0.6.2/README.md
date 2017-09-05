This crate provides an implementation of the
[Aho-Corasick](http://en.wikipedia.org/wiki/Aho%E2%80%93Corasick_string_matching_algorithm)
algorithm. Its intended use case is for fast substring matching, particularly
when matching multiple substrings in a search text. This is achieved by
compiling the substrings into a finite state machine.

This implementation provides optimal algorithmic time complexity. Construction
of the finite state machine is `O(p)` where `p` is the length of the substrings
concatenated. Matching against search text is `O(n + p + m)`, where `n` is
the length of the search text and `m` is the number of matches.

[![Build status](https://api.travis-ci.org/BurntSushi/aho-corasick.png)](https://travis-ci.org/BurntSushi/aho-corasick)
[![](http://meritbadge.herokuapp.com/aho-corasick)](https://crates.io/crates/aho-corasick)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Documentation

[http://burntsushi.net/rustdoc/aho_corasick/](http://burntsushi.net/rustdoc/aho_corasick/).


### Example

The documentation contains several examples, and there is a more complete
example as a full program in `examples/dict-search.rs`.

Here is a quick example showing simple substring matching:

```rust
use aho_corasick::{Automaton, AcAutomaton, Match};

let aut = AcAutomaton::new(vec!["apple", "maple"]);
let mut it = aut.find("I like maple apples.");
assert_eq!(it.next(), Some(Match {
    pati: 1,
    start: 7,
    end: 12,
}));
assert_eq!(it.next(), Some(Match {
    pati: 0,
    start: 13,
    end: 18,
}));
assert_eq!(it.next(), None);
```


### Alternatives

Aho-Corasick is useful for matching multiple substrings against many long
strings. If your long string is fixed, then you might consider building a
[suffix array](https://github.com/BurntSushi/suffix)
of the search text (which takes `O(n)` time). Matches can then be found in
`O(plogn)` time.
