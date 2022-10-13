C API for RUst's REgex engine
=============================
rure is a C API to Rust's regex library, which guarantees linear time
searching using finite automata. In exchange, it must give up some common
regex features such as backreferences and arbitrary lookaround. It does
however include capturing groups, lazy matching, Unicode support and word
boundary assertions. Its matching semantics generally correspond to Perl's,
or "leftmost first." Namely, the match locations reported correspond to the
first match that would be found by a backtracking engine.

The header file (`includes/rure.h`) serves as the primary API documentation of
this library. Types and flags are documented first, and functions follow.

The syntax and possibly other useful things are documented in the Rust
API documentation: https://docs.rs/regex


Examples
--------
There are readable examples in the `ctest` and `examples` sub-directories.

Assuming you have
[Rust and Cargo installed](https://www.rust-lang.org/downloads.html)
(and a C compiler), then this should work to run the `iter` example:

```
$ git clone git://github.com/rust-lang/regex
$ cd regex/regex-capi/examples
$ ./compile
$ LD_LIBRARY_PATH=../target/release ./iter
```


Performance
-----------
It's fast. Its core matching engine is a lazy DFA, which is what GNU grep
and RE2 use. Like GNU grep, this regex engine can detect multi byte literals
in the regex and will use fast literal string searching to quickly skip
through the input to find possible match locations.

All memory usage is bounded and all searching takes linear time with respect
to the input string.

For more details, see the PERFORMANCE guide:
https://github.com/rust-lang/regex/blob/master/PERFORMANCE.md


Text encoding
-------------
All regular expressions must be valid UTF-8.

The text encoding of haystacks is more complicated. To a first
approximation, haystacks should be UTF-8. In fact, UTF-8 (and, one
supposes, ASCII) is the only well defined text encoding supported by this
library. It is impossible to match UTF-16, UTF-32 or any other encoding
without first transcoding it to UTF-8.

With that said, haystacks do not need to be valid UTF-8, and if they aren't
valid UTF-8, no performance penalty is paid. Whether invalid UTF-8 is
matched or not depends on the regular expression. For example, with the
`RURE_FLAG_UNICODE` flag enabled, the regex `.` is guaranteed to match a
single UTF-8 encoding of a Unicode codepoint (sans LF). In particular,
it will not match invalid UTF-8 such as `\xFF`, nor will it match surrogate
codepoints or "alternate" (i.e., non-minimal) encodings of codepoints.
However, with the `RURE_FLAG_UNICODE` flag disabled, the regex `.` will match
any *single* arbitrary byte (sans LF), including `\xFF`.

This provides a useful invariant: wherever `RURE_FLAG_UNICODE` is set, the
corresponding regex is guaranteed to match valid UTF-8. Invalid UTF-8 will
always prevent a match from happening when the flag is set. Since flags can be
toggled in the regular expression itself, this allows one to pick and choose
which parts of the regular expression must match UTF-8 or not.

Some good advice is to always enable the `RURE_FLAG_UNICODE` flag (which is
enabled when using `rure_compile_must`) and selectively disable the flag when
one wants to match arbitrary bytes. The flag can be disabled in a regular
expression with `(?-u)`.

Finally, if one wants to match specific invalid UTF-8 bytes, then you can
use escape sequences. e.g., `(?-u)\\xFF` will match `\xFF`. It's not
possible to use C literal escape sequences in this case since regular
expressions must be valid UTF-8.


Aborts
------
This library will abort your process if an unwinding panic is caught in the
Rust code. Generally, a panic occurs when there is a bug in the program or
if allocation failed. It is possible to cause this behavior by passing
invalid inputs to some functions. For example, giving an invalid capture
group index to `rure_captures_at` will cause Rust's bounds checks to fail,
which will cause a panic, which will be caught and printed to stderr. The
process will then `abort`.


Missing
-------
There are a few things missing from the C API that are present in the Rust API.
There's no particular (known) reason why they don't, they just haven't been
implemented yet.

* Splitting a string by a regex.
* Replacing regex matches in a string with some other text.
