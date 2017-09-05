0.2.2 (2017-05-21)
==================
New features:

* [FEATURE #341](https://github.com/rust-lang/regex/issues/341):
  Support nested character classes and intersection operation.
  For example, `[\p{Greek}&&\pL]` matches greek letters and
  `[[0-9]&&[^4]]` matches every decimal digit except `4`.
  (Much thanks to @robinst, who contributed this awesome feature.)

Bug fixes:

* [BUG #321](https://github.com/rust-lang/regex/issues/321):
  Fix bug in literal extraction and UTF-8 decoding.
* [BUG #326](https://github.com/rust-lang/regex/issues/326):
  Add documentation tip about the `(?x)` flag.
* [BUG #333](https://github.com/rust-lang/regex/issues/333):
  Show additional replacement example using curly braces.
* [BUG #334](https://github.com/rust-lang/regex/issues/334):
  Fix bug when resolving captures after a match.
* [BUG #338](https://github.com/rust-lang/regex/issues/338):
  Add example that uses `Captures::get` to API documentation.
* [BUG #353](https://github.com/rust-lang/regex/issues/353):
  Fix RegexSet bug that caused match failure in some cases.
* [BUG #354](https://github.com/rust-lang/regex/pull/354):
  Fix panic in parser when `(?x)` is used.
* [BUG #358](https://github.com/rust-lang/regex/issues/358):
  Fix literal optimization bug with RegexSet.
* [BUG #359](https://github.com/rust-lang/regex/issues/359):
  Fix example code in README.
* [BUG #365](https://github.com/rust-lang/regex/pull/365):
  Fix bug in `rure_captures_len` in the C binding.
* [BUG #367](https://github.com/rust-lang/regex/issues/367):
  Fix byte class bug that caused a panic.


0.2.1
=====
One major bug with `replace_all` has been fixed along with a couple of other
touchups.

* [BUG #312](https://github.com/rust-lang/regex/issues/312):
  Fix documentation for `NoExpand` to reference correct lifetime parameter.
* [BUG #314](https://github.com/rust-lang/regex/issues/314):
  Fix a bug with `replace_all` when replacing a match with the empty string.
* [BUG #316](https://github.com/rust-lang/regex/issues/316):
  Note a missing breaking change from the `0.2.0` CHANGELOG entry.
  (`RegexBuilder::compile` was renamed to `RegexBuilder::build`.)
* [BUG #324](https://github.com/rust-lang/regex/issues/324):
  Compiling `regex` should only require one version of `memchr` crate.


0.2.0
=====
This is a new major release of the regex crate, and is an implementation of the
[regex 1.0 RFC](https://github.com/rust-lang/rfcs/blob/master/text/1620-regex-1.0.md).
We are releasing a `0.2` first, and if there are no major problems, we will
release a `1.0` shortly. For `0.2`, the minimum *supported* Rust version is
1.12.

There are a number of **breaking changes** in `0.2`. They are split into two
types. The first type correspond to breaking changes in regular expression
syntax. The second type correspond to breaking changes in the API.

Breaking changes for regex syntax:

* POSIX character classes now require double bracketing. Previously, the regex
  `[:upper:]` would parse as the `upper` POSIX character class. Now it parses
  as the character class containing the characters `:upper:`. The fix to this
  change is to use `[[:upper:]]` instead. Note that variants like
  `[[:upper:][:blank:]]` continue to work.
* The character `[` must always be escaped inside a character class.
* The characters `&`, `-` and `~` must be escaped if any one of them are
  repeated consecutively. For example, `[&]`, `[\&]`, `[\&\&]`, `[&-&]` are all
  equivalent while `[&&]` is illegal. (The motivation for this and the prior
  change is to provide a backwards compatible path for adding character class
  set notation.)
* A `bytes::Regex` now has Unicode mode enabled by default (like the main
  `Regex` type). This means regexes compiled with `bytes::Regex::new` that
  don't have the Unicode flag set should add `(?-u)` to recover the original
  behavior.

Breaking changes for the regex API:

* `find` and `find_iter` now **return `Match` values instead of
  `(usize, usize)`.** `Match` values have `start` and `end` methods, which
  return the match offsets. `Match` values also have an `as_str` method,
  which returns the text of the match itself.
* The `Captures` type now only provides a single iterator over all capturing
  matches, which should replace uses of `iter` and `iter_pos`. Uses of
  `iter_named` should use the `capture_names` method on `Regex`.
* The `at` method on the `Captures` type has been renamed to `get`, and it
  now returns a `Match`. Similarly, the `name` method on `Captures` now returns
  a `Match`.
* The `replace` methods now return `Cow` values. The `Cow::Borrowed` variant
  is returned when no replacements are made.
* The `Replacer` trait has been completely overhauled. This should only
  impact clients that implement this trait explicitly. Standard uses of
  the `replace` methods should continue to work unchanged. If you implement
  the `Replacer` trait, please consult the new documentation.
* The `quote` free function has been renamed to `escape`.
* The `Regex::with_size_limit` method has been removed. It is replaced by
  `RegexBuilder::size_limit`.
* The `RegexBuilder` type has switched from owned `self` method receivers to
  `&mut self` method receivers. Most uses will continue to work unchanged, but
  some code may require naming an intermediate variable to hold the builder.
* The `compile` method on `RegexBuilder` has been renamed to `build`.
* The free `is_match` function has been removed. It is replaced by compiling
  a `Regex` and calling its `is_match` method.
* The `PartialEq` and `Eq` impls on `Regex` have been dropped. If you relied
  on these impls, the fix is to define a wrapper type around `Regex`, impl
  `Deref` on it and provide the necessary impls.
* The `is_empty` method on `Captures` has been removed. This always returns
  `false`, so its use is superfluous.
* The `Syntax` variant of the `Error` type now contains a string instead of
  a `regex_syntax::Error`. If you were examining syntax errors more closely,
  you'll need to explicitly use the `regex_syntax` crate to re-parse the regex.
* The `InvalidSet` variant of the `Error` type has been removed since it is
  no longer used.
* Most of the iterator types have been renamed to match conventions. If you
  were using these iterator types explicitly, please consult the documentation
  for its new name. For example, `RegexSplits` has been renamed to `Split`.

A number of bugs have been fixed:

* [BUG #151](https://github.com/rust-lang/regex/issues/151):
  The `Replacer` trait has been changed to permit the caller to control
  allocation.
* [BUG #165](https://github.com/rust-lang/regex/issues/165):
  Remove the free `is_match` function.
* [BUG #166](https://github.com/rust-lang/regex/issues/166):
  Expose more knobs (available in `0.1`) and remove `with_size_limit`.
* [BUG #168](https://github.com/rust-lang/regex/issues/168):
  Iterators produced by `Captures` now have the correct lifetime parameters.
* [BUG #175](https://github.com/rust-lang/regex/issues/175):
  Fix a corner case in the parsing of POSIX character classes.
* [BUG #178](https://github.com/rust-lang/regex/issues/178):
  Drop the `PartialEq` and `Eq` impls on `Regex`.
* [BUG #179](https://github.com/rust-lang/regex/issues/179):
  Remove `is_empty` from `Captures` since it always returns false.
* [BUG #276](https://github.com/rust-lang/regex/issues/276):
  Position of named capture can now be retrieved from a `Captures`.
* [BUG #296](https://github.com/rust-lang/regex/issues/296):
  Remove winapi/kernel32-sys dependency on UNIX.
* [BUG #307](https://github.com/rust-lang/regex/issues/307):
  Fix error on emscripten.


0.1.80
======
* [PR #292](https://github.com/rust-lang/regex/pull/292):
  Fixes bug #291, which was introduced by PR #290.

0.1.79
======
* Require regex-syntax 0.3.8.

0.1.78
======
* [PR #290](https://github.com/rust-lang/regex/pull/290):
  Fixes bug #289, which caused some regexes with a certain combination
  of literals to match incorrectly.

0.1.77
======
* [PR #281](https://github.com/rust-lang/regex/pull/281):
  Fixes bug #280 by disabling all literal optimizations when a pattern
  is partially anchored.

0.1.76
======
* Tweak criteria for using the Teddy literal matcher.

0.1.75
======
* [PR #275](https://github.com/rust-lang/regex/pull/275):
  Improves match verification performance in the Teddy SIMD searcher.
* [PR #278](https://github.com/rust-lang/regex/pull/278):
  Replaces slow substring loop in the Teddy SIMD searcher with Aho-Corasick.
* Implemented DoubleEndedIterator on regex set match iterators.

0.1.74
======
* Release regex-syntax 0.3.5 with a minor bug fix.
* Fix bug #272.
* Fix bug #277.
* [PR #270](https://github.com/rust-lang/regex/pull/270):
  Fixes bugs #264, #268 and an unreported where the DFA cache size could be
  drastically under estimated in some cases (leading to high unexpected memory
  usage).

0.1.73
======
* Release `regex-syntax 0.3.4`.
* Bump `regex-syntax` dependency version for `regex` to `0.3.4`.

0.1.72
======
* [PR #262](https://github.com/rust-lang/regex/pull/262):
  Fixes a number of small bugs caught by fuzz testing (AFL).

0.1.71
======
* [PR #236](https://github.com/rust-lang/regex/pull/236):
  Fix a bug in how suffix literals were extracted, which could lead
  to invalid match behavior in some cases.

0.1.70
======
* [PR #231](https://github.com/rust-lang/regex/pull/231):
  Add SIMD accelerated multiple pattern search.
* [PR #228](https://github.com/rust-lang/regex/pull/228):
  Reintroduce the reverse suffix literal optimization.
* [PR #226](https://github.com/rust-lang/regex/pull/226):
  Implements NFA state compression in the lazy DFA.
* [PR #223](https://github.com/rust-lang/regex/pull/223):
  A fully anchored RegexSet can now short-circuit.

0.1.69
======
* [PR #216](https://github.com/rust-lang/regex/pull/216):
  Tweak the threshold for running backtracking.
* [PR #217](https://github.com/rust-lang/regex/pull/217):
  Add upper limit (from the DFA) to capture search (for the NFA).
* [PR #218](https://github.com/rust-lang/regex/pull/218):
  Add rure, a C API.

0.1.68
======
* [PR #210](https://github.com/rust-lang/regex/pull/210):
  Fixed a performance bug in `bytes::Regex::replace` where `extend` was used
  instead of `extend_from_slice`.
* [PR #211](https://github.com/rust-lang/regex/pull/211):
  Fixed a bug in the handling of word boundaries in the DFA.
* [PR #213](https://github.com/rust-lang/pull/213):
  Added RE2 and Tcl to the benchmark harness. Also added a CLI utility from
  running regexes using any of the following regex engines: PCRE1, PCRE2,
  Oniguruma, RE2, Tcl and of course Rust's own regexes.

0.1.67
======
* [PR #201](https://github.com/rust-lang/regex/pull/201):
  Fix undefined behavior in the `regex!` compiler plugin macro.
* [PR #205](https://github.com/rust-lang/regex/pull/205):
  More improvements to DFA performance. Competitive with RE2. See PR for
  benchmarks.
* [PR #209](https://github.com/rust-lang/regex/pull/209):
  Release 0.1.66 was semver incompatible since it required a newer version
  of Rust than previous releases. This PR fixes that. (And `0.1.66` was
  yanked.)

0.1.66
======
* Speculative support for Unicode word boundaries was added to the DFA. This
  should remove the last common case that disqualified use of the DFA.
* An optimization that scanned for suffix literals and then matched the regular
  expression in reverse was removed because it had worst case quadratic time
  complexity. It was replaced with a more limited optimization where, given any
  regex of the form `re$`, it will be matched in reverse from the end of the
  haystack.
* [PR #202](https://github.com/rust-lang/regex/pull/202):
  The inner loop of the DFA was heavily optimized to improve cache locality
  and reduce the overall number of instructions run on each iteration. This
  represents the first use of `unsafe` in `regex` (to elide bounds checks).
* [PR #200](https://github.com/rust-lang/regex/pull/200):
  Use of the `mempool` crate (which used thread local storage) was replaced
  with a faster version of a similar API in @Amanieu's `thread_local` crate.
  It should reduce contention when using a regex from multiple threads
  simultaneously.
* PCRE2 JIT benchmarks were added. A benchmark comparison can be found
  [here](https://gist.github.com/anonymous/14683c01993e91689f7206a18675901b).
  (Includes a comparison with PCRE1's JIT and Oniguruma.)
* A bug where word boundaries weren't being matched correctly in the DFA was
  fixed. This only affected use of `bytes::Regex`.
* [#160](https://github.com/rust-lang/regex/issues/160):
  `Captures` now has a `Debug` impl.
