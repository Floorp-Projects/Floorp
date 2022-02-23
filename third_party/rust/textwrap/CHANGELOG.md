# Changelog

This file lists the most important changes made in each release of
`textwrap`.

## Version 0.14.2 (2021-06-27)

The 0.14.1 release included more changes than intended and has been
yanked. The change intended for 0.14.1 is now included in 0.14.2.

## Version 0.14.1 (2021-06-26)

This release fixes a panic reported by @Makoto, thanks!

* [#391](https://github.com/mgeisler/textwrap/pull/391): Fix panic in
  `find_words` due to string access outside of a character boundary.

## Version 0.14.0 (2021-06-05)

This is a major feature release which makes Textwrap more configurable
and flexible. The high-level API of `textwrap::wrap` and
`textwrap::fill` remains unchanged, but low-level structs have moved
around.

The biggest change is the introduction of new generic type parameters
to the `Options` struct. These parameters lets you statically
configure the wrapping algorithm, the word separator, and the word
splitter. If you previously spelled out the full type for `Options`,
you now need to take the extra type parameters into account. This
means that

```rust
let options: Options<HyphenSplitter> = Options::new(80);
```

changes to

```rust
let options: Options<
    wrap_algorithms::FirstFit,
    word_separators::AsciiSpace,
    word_splitters::HyphenSplitter,
> = Options::new(80);
```

This is quite a mouthful, so we suggest using type inferrence where
possible. You won’t see any chance if you call `wrap` directly with a
width or with an `Options` value constructed on the fly. Please open
an issue if this causes problems for you!

### New `WordSeparator` Trait

* [#332](https://github.com/mgeisler/textwrap/pull/332): Add
  `WordSeparator` trait to allow customizing how words are found in a
  line of text. Until now, Textwrap would always assume that words are
  separated by ASCII space characters. You can now customize this as
  needed.

* [#313](https://github.com/mgeisler/textwrap/pull/313): Add support
  for using the Unicode line breaking algorithm to find words. This is
  done by adding a second implementation of the new `WordSeparator`
  trait. The implementation uses the unicode-linebreak crate, which is
  a new optional dependency.

  With this, Textwrap can be used with East-Asian languages such as
  Chinese or Japanese where there are no spaces between words.
  Breaking a long sequence of emojis is another example where line
  breaks might be wanted even if there are no whitespace to be found.
  Feedback would be appreciated for this feature.


### Indent

* [#353](https://github.com/mgeisler/textwrap/pull/353): Trim trailing
  whitespace from `prefix` in `indent`.

  Before, empty lines would get no prefix added. Now, empty lines have
  a trimmed prefix added. This little trick makes `indent` much more
  useful since you can now safely indent with `"# "` without creating
  trailing whitespace in the output due to the trailing whitespace in
  your prefix.

* [#354](https://github.com/mgeisler/textwrap/pull/354): Make `indent`
  about 20% faster by preallocating the output string.


### Documentation

* [#308](https://github.com/mgeisler/textwrap/pull/308): Document
  handling of leading and trailing whitespace when wrapping text.

### WebAssembly Demo

* [#310](https://github.com/mgeisler/textwrap/pull/310): Thanks to
  WebAssembly, you can now try out Textwrap directly in your browser.
  Please try it out: https://mgeisler.github.io/textwrap/.

### New Generic Parameters

* [#331](https://github.com/mgeisler/textwrap/pull/331): Remove outer
  boxing from `Options`.

* [#357](https://github.com/mgeisler/textwrap/pull/357): Replace
  `core::WrapAlgorithm` enum with a `wrap_algorithms::WrapAlgorithm`
  trait. This allows for arbitrary wrapping algorithms to be plugged
  into the library.

* [#358](https://github.com/mgeisler/textwrap/pull/358): Switch
  wrapping functions to use a slice for `line_widths`.

* [#368](https://github.com/mgeisler/textwrap/pull/368): Move
  `WordSeparator` and `WordSplitter` traits to separate modules.
  Before, Textwrap had several top-level structs such as
  `NoHyphenation` and `HyphenSplitter`. These implementations of
  `WordSplitter` now lives in a dedicated `word_splitters` module.
  Similarly, we have a new `word_separators` module for
  implementations of `WordSeparator`.

* [#369](https://github.com/mgeisler/textwrap/pull/369): Rename
  `Options::splitter` to `Options::word_splitter` for consistency with
  the other fields backed by traits.

## Version 0.13.4 (2021-02-23)

This release removes `println!` statements which was left behind in
`unfill` by mistake.

* [#296](https://github.com/mgeisler/textwrap/pull/296): Improve house
  building example with more comments.
* [#297](https://github.com/mgeisler/textwrap/pull/297): Remove debug
  prints in the new `unfill` function.

## Version 0.13.3 (2021-02-20)

This release contains a bugfix for `indent` and improved handling of
emojis. We’ve also added a new function for formatting text in columns
and functions for reformatting already wrapped text.

* [#276](https://github.com/mgeisler/textwrap/pull/276): Extend
  `core::display_width` to handle emojis when the unicode-width Cargo
  feature is disabled.
* [#279](https://github.com/mgeisler/textwrap/pull/279): Make `indent`
  preserve existing newlines in the input string. Before,
  `indent("foo", "")` would return `"foo\n"` by mistake. It now
  returns `"foo"` instead.
* [#281](https://github.com/mgeisler/textwrap/pull/281): Ensure all
  `Options` fields have examples.
* [#282](https://github.com/mgeisler/textwrap/pull/282): Add a
  `wrap_columns` function.
* [#294](https://github.com/mgeisler/textwrap/pull/294): Add new
  `unfill` and `refill` functions.

## Version 0.13.2 (2020-12-30)

This release primarily makes all dependencies optional. This makes it
possible to slim down textwrap as needed.

* [#254](https://github.com/mgeisler/textwrap/pull/254): `impl
  WordSplitter` for `Box<T> where T: WordSplitter`.
* [#255](https://github.com/mgeisler/textwrap/pull/255): Use command
  line arguments as initial text in interactive example.
* [#256](https://github.com/mgeisler/textwrap/pull/256): Introduce
  fuzz tests for `wrap_optimal_fit` and `wrap_first_fit`.
* [#260](https://github.com/mgeisler/textwrap/pull/260): Make the
  unicode-width dependency optional.
* [#261](https://github.com/mgeisler/textwrap/pull/261): Make the
  smawk dependency optional.

## Version 0.13.1 (2020-12-10)

This is a bugfix release which fixes a regression in 0.13.0. The bug
meant that colored text was wrapped incorrectly.

* [#245](https://github.com/mgeisler/textwrap/pull/245): Support
  deleting a word with Ctrl-Backspace in the interactive demo.
* [#246](https://github.com/mgeisler/textwrap/pull/246): Show build
  type (debug/release) in interactive demo.
* [#249](https://github.com/mgeisler/textwrap/pull/249): Correctly
  compute width while skipping over ANSI escape sequences.

## Version 0.13.0 (2020-12-05)

This is a major release which rewrites the core logic, adds many new
features, and fixes a couple of bugs. Most programs which use
`textwrap` stays the same, incompatibilities and upgrade notes are
given below.

Clone the repository and run the following to explore the new features
in an interactive demo (Linux only):

```sh
$ cargo run --example interactive --all-features
```

### Bug Fixes

#### Rewritten core wrapping algorithm

* [#221](https://github.com/mgeisler/textwrap/pull/221): Reformulate
  wrapping in terms of words with whitespace and penalties.

The core wrapping algorithm has been completely rewritten. This fixed
bugs and simplified the code, while also making it possible to use
`textwrap` outside the context of the terminal.

As part of this, trailing whitespace is now discarded consistently
from wrapped lines. Before we would inconsistently remove whitespace
at the end of wrapped lines, except for the last. Leading whitespace
is still preserved.

### New Features

#### Optimal-fit wrapping

* [#234](https://github.com/mgeisler/textwrap/pull/234): Introduce
  wrapping using an optimal-fit algorithm.

This release adds support for new wrapping algorithm which finds a
globally optimal set of line breaks, taking certain penalties into
account. As an example, the old algorithm would produce

    "To be, or"
    "not to be:"
    "that is"
    "the"
    "question"

Notice how the fourth line with “the” is very short. The new algorithm
shortens the previous lines slightly to produce fewer short lines:

    "To be,"
    "or not to"
    "be: that"
    "is the"
    "question"

Use the new `textwrap::core::WrapAlgorithm` enum to select between the
new and old algorithm. By default, the new algorithm is used.

The optimal-fit algorithm is inspired by the line breaking algorithm
used in TeX, described in the 1981 article [_Breaking Paragraphs into
Lines_](http://www.eprg.org/G53DOC/pdfs/knuth-plass-breaking.pdf) by
Knuth and Plass.

#### In-place wrapping

* [#226](https://github.com/mgeisler/textwrap/pull/226): Add a
  `fill_inplace` function.

When the text you want to fill is already a temporary `String`, you
can now mutate it in-place with `fill_inplace`:

```rust
let mut greeting = format!("Greetings {}, welcome to the game! You have {} lives left.",
                           player.name, player.lives);
fill_inplace(&mut greeting, line_width);
```

This is faster than calling `fill` and it will reuse the memory
already allocated for the string.

### Changed Features

#### `Wrapper` is replaced with `Options`

* [#213](https://github.com/mgeisler/textwrap/pull/213): Simplify API
  with only top-level functions.
* [#215](https://github.com/mgeisler/textwrap/pull/215): Reintroducing
  the type parameter on `Options` (previously known as `Wrapper`).
* [#219](https://github.com/mgeisler/textwrap/pull/219): Allow using
  trait objects with `fill` & `wrap`.
* [#227](https://github.com/mgeisler/textwrap/pull/227): Replace
  `WrapOptions` with `Into<Options>`.

The `Wrapper` struct held the options (line width, indentation, etc)
for wrapping text. It was also the entry point for actually wrapping
the text via its methods such as `wrap`, `wrap_iter`,
`into_wrap_iter`, and `fill` methods.

The struct has been replaced by a simpler `Options` struct which only
holds options. The `Wrapper` methods are gone, their job has been
taken over by the top-level `wrap` and `fill` functions. The signature
of these functions have changed from

```rust
fn fill(s: &str, width: usize) -> String;

fn wrap(s: &str, width: usize) -> Vec<Cow<'_, str>>;
```

to the more general

```rust
fn fill<'a, S, Opt>(text: &str, options: Opt) -> String
where
    S: WordSplitter,
    Opt: Into<Options<'a, S>>;

fn wrap<'a, S, Opt>(text: &str, options: Opt) -> Vec<Cow<'_, str>>
where
    S: WordSplitter,
    Opt: Into<Options<'a, S>>;
```

The `Into<Options<'a, S>` bound allows you to pass an `usize` (which
is interpreted as the line width) *and* a full `Options` object. This
allows the new functions to work like the old, plus you can now fully
customize the behavior of the wrapping via `Options` when needed.

Code that call `textwrap::wrap` or `textwrap::fill` can remain
unchanged. Code that calls into `Wrapper::wrap` or `Wrapper::fill`
will need to be update. This is a mechanical change, please see
[#213](https://github.com/mgeisler/textwrap/pull/213) for examples.

Thanks to @CryptJar and @Koxiat for their support in the PRs above!

### Removed Features

* The `wrap_iter` and `into_wrap_iter` methods are gone. This means
  that lazy iteration is no longer supported: you always get all
  wrapped lines back as a `Vec`. This was done to simplify the code
  and to support the optimal-fit algorithm.

  The first-fit algorithm could still be implemented in an incremental
  fashion. Please let us know if this is important to you.

### Other Changes

* [#206](https://github.com/mgeisler/textwrap/pull/206): Change
  `Wrapper.splitter` from `T: WordSplitter` to `Box<dyn
  WordSplitter>`.
* [#216](https://github.com/mgeisler/textwrap/pull/216): Forbid the
  use of unsafe code.

## Version 0.12.1 (2020-07-03)

This is a bugfix release.

* Fixed [#176][issue-176]: Mention compile-time wrapping by linking to
  the [`textwrap-macros` crate].
* Fixed [#193][issue-193]: Wrapping with `break_words(false)` was
  broken and would cause extra whitespace to be inserted when words
  were longer than the line width.

## Version 0.12.0 (2020-06-26)

The code has been updated to the [Rust 2018 edition][rust-2018] and
each new release of `textwrap` will only support the latest stable
version of Rust. Trying to support older Rust versions is a fool's
errand: our dependencies keep releasing new patch versions that
require newer and newer versions of Rust.

The `term_size` feature has been replaced by `terminal_size`. The API
is unchanged, it is just the name of the Cargo feature that changed.

The `hyphenation` feature now only embeds the hyphenation patterns for
US-English. This slims down the dependency.

* Fixed [#140][issue-140]: Ignore ANSI escape sequences.
* Fixed [#158][issue-158]: Unintended wrapping when using external splitter.
* Fixed [#177][issue-177]: Update examples to the 2018 edition.

## Version 0.11.0 (2018-12-09)

Due to our dependencies bumping their minimum supported version of
Rust, the minimum version of Rust we test against is now 1.22.0.

* Merged [#141][issue-141]: Fix `dedent` handling of empty lines and
  trailing newlines. Thanks @bbqsrc!
* Fixed [#151][issue-151]: Release of version with hyphenation 0.7.

## Version 0.10.0 (2018-04-28)

Due to our dependencies bumping their minimum supported version of
Rust, the minimum version of Rust we test against is now 1.17.0.

* Fixed [#99][issue-99]: Word broken even though it would fit on line.
* Fixed [#107][issue-107]: Automatic hyphenation is off by one.
* Fixed [#122][issue-122]: Take newlines into account when wrapping.
* Fixed [#129][issue-129]: Panic on string with em-dash.

## Version 0.9.0 (2017-10-05)

The dependency on `term_size` is now optional, and by default this
feature is not enabled. This is a *breaking change* for users of
`Wrapper::with_termwidth`. Enable the `term_size` feature to restore
the old functionality.

Added a regression test for the case where `width` is set to
`usize::MAX`, thanks @Fraser999! All public structs now implement
`Debug`, thanks @hcpl!

* Fixed [#101][issue-101]: Make `term_size` an optional dependency.

## Version 0.8.0 (2017-09-04)

The `Wrapper` stuct is now generic over the type of word splitter
being used. This means less boxing and a nicer API. The
`Wrapper::word_splitter` method has been removed. This is a *breaking
API change* if you used the method to change the word splitter.

The `Wrapper` struct has two new methods that will wrap the input text
lazily: `Wrapper::wrap_iter` and `Wrapper::into_wrap_iter`. Use those
if you will be iterating over the wrapped lines one by one.

* Fixed [#59][issue-59]: `wrap` could return an iterator. Thanks
  @hcpl!
* Fixed [#81][issue-81]: Set `html_root_url`.

## Version 0.7.0 (2017-07-20)

Version 0.7.0 changes the return type of `Wrapper::wrap` from
`Vec<String>` to `Vec<Cow<'a, str>>`. This means that the output lines
borrow data from the input string. This is a *breaking API change* if
you relied on the exact return type of `Wrapper::wrap`. Callers of the
`textwrap::fill` convenience function will see no breakage.

The above change and other optimizations makes version 0.7.0 roughly
15-30% faster than version 0.6.0.

The `squeeze_whitespace` option has been removed since it was
complicating the above optimization. Let us know if this option is
important for you so we can provide a work around.

* Fixed [#58][issue-58]: Add a "fast_wrap" function.
* Fixed [#61][issue-61]: Documentation errors.

## Version 0.6.0 (2017-05-22)

Version 0.6.0 adds builder methods to `Wrapper` for easy one-line
initialization and configuration:

```rust
let wrapper = Wrapper::new(60).break_words(false);
```

It also add a new `NoHyphenation` word splitter that will never split
words, not even at existing hyphens.

* Fixed [#28][issue-28]: Support not squeezing whitespace.

## Version 0.5.0 (2017-05-15)

Version 0.5.0 has *breaking API changes*. However, this only affects
code using the hyphenation feature. The feature is now optional, so
you will first need to enable the `hyphenation` feature as described
above. Afterwards, please change your code from
```rust
wrapper.corpus = Some(&corpus);
```
to
```rust
wrapper.splitter = Box::new(corpus);
```

Other changes include optimizations, so version 0.5.0 is roughly
10-15% faster than version 0.4.0.

* Fixed [#19][issue-19]: Add support for finding terminal size.
* Fixed [#25][issue-25]: Handle words longer than `self.width`.
* Fixed [#26][issue-26]: Support custom indentation.
* Fixed [#36][issue-36]: Support building without `hyphenation`.
* Fixed [#39][issue-39]: Respect non-breaking spaces.

## Version 0.4.0 (2017-01-24)

Documented complexities and tested these via `cargo bench`.

* Fixed [#13][issue-13]: Immediatedly add word if it fits.
* Fixed [#14][issue-14]: Avoid splitting on initial hyphens.

## Version 0.3.0 (2017-01-07)

Added support for automatic hyphenation.

## Version 0.2.0 (2016-12-28)

Introduced `Wrapper` struct. Added support for wrapping on hyphens.

## Version 0.1.0 (2016-12-17)

First public release with support for wrapping strings on whitespace.

[rust-2018]: https://doc.rust-lang.org/edition-guide/rust-2018/
[`textwrap-macros` crate]: https://crates.io/crates/textwrap-macros

[issue-13]: https://github.com/mgeisler/textwrap/issues/13
[issue-14]: https://github.com/mgeisler/textwrap/issues/14
[issue-19]: https://github.com/mgeisler/textwrap/issues/19
[issue-25]: https://github.com/mgeisler/textwrap/issues/25
[issue-26]: https://github.com/mgeisler/textwrap/issues/26
[issue-28]: https://github.com/mgeisler/textwrap/issues/28
[issue-36]: https://github.com/mgeisler/textwrap/issues/36
[issue-39]: https://github.com/mgeisler/textwrap/issues/39
[issue-58]: https://github.com/mgeisler/textwrap/issues/58
[issue-59]: https://github.com/mgeisler/textwrap/issues/59
[issue-61]: https://github.com/mgeisler/textwrap/issues/61
[issue-81]: https://github.com/mgeisler/textwrap/issues/81
[issue-99]: https://github.com/mgeisler/textwrap/issues/99
[issue-101]: https://github.com/mgeisler/textwrap/issues/101
[issue-107]: https://github.com/mgeisler/textwrap/issues/107
[issue-122]: https://github.com/mgeisler/textwrap/issues/122
[issue-129]: https://github.com/mgeisler/textwrap/issues/129
[issue-140]: https://github.com/mgeisler/textwrap/issues/140
[issue-141]: https://github.com/mgeisler/textwrap/issues/141
[issue-151]: https://github.com/mgeisler/textwrap/issues/151
[issue-158]: https://github.com/mgeisler/textwrap/issues/158
[issue-176]: https://github.com/mgeisler/textwrap/issues/176
[issue-177]: https://github.com/mgeisler/textwrap/issues/177
[issue-193]: https://github.com/mgeisler/textwrap/issues/193
