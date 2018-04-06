# 0.11.2

## Non-breaking changes:
- Added class_cases and removed -- Thanks @Yoric
  - Use pascal case
- Updated lazy_static to 1.0.0

# 0.11.1

## Non-breaking changes:
- Fixed an issue where words ending in e.g. "-ches", such as "witches"; that
  would be singularized as "wit" instead of the expected "witch". -- Thanks nbaksalyar
- Will be removing ascii import when current nightly goes stable.

# 0.11.0

## Breaking changes:
- Made snake case deal correctly with special characters. Behaviour now follows
  rails infector
- Made camel case deal correctly with special characters. Behaviour now follows
  rails infector

## Non-breaking changes:
- Removed magic macros for testing.
- Added explicit tests for all cases.

# 0.10.1

## Non-breaking changes:
- Added flags for unused macros. Any current macros with this flag with either
  be moved or removed.

# 0.10.0

## Non-breaking changes:
- Changed from `fold` to `for in` which resulted in a average 10-20% boost in
  performance for all castings using the case module.

## Fixes:
- Fixed issue with snake case like strings where numbers were incorrectly
  permitted to be next to a string e.g. `string1` was considered valid when it
  should have been `string_1`. This has been corrected as part of the above
  change.

## Why is this not 0.9.1?
- The change in this case are all on private API. This normally wouldn't cause a
  breaking change as there are extensive tests wrapping all methods. This
  however doesn't preclude an edge case that hasn't been considered. I've
  deemed it safer to call this a 0.x.0 release for that reason.

# 0.9.0

## Breaking changes:
- Changed type signature for all casting methods to use `&str`. This gives the
  crate a >10% increase in performance in a majority of conversions. Although
  most users only use:
     `"my_string".to_camel_case()`
  or
    `"myString".to_string().to_snake_case()`
  For those using the `to_camel_case("something".to_string())` this will be a
  breaking change as the new syntax demands `to_camel_case("something")`.


# 0.8.1

## Fixes:

- Fixed singularize issues with `/ies/`. Thanks @mthjones
- Fixed issue with feature gates which may have caused downstream api
  incompatibilities.

# 0.8.0

## New features:

- Feature gated pluralize, singularize, class_case, table_case, demodulize, and
  deconstantize. This can be activated by passing --features=lightweight. See
  README


## Possible breaking change:

- Feature gated items are on by default, meaning that you'll get the full
  version of the crate if you install as normal. See README if you want to use
  the lightweight version.
- Although the application still passes all tests, substantial portions of the
  core of the conversion code have been method extracted and may have caused a
  change for some people. Please file an issue if this is a problem for you.

# 0.7.0

## New features:

- Added traits for various number types on ordinalize.

## Possible breaking change:

- Fixed issue with Boxes to Box which may cause breakages for some people

## Non-breaking change:

- Updated dependencies on `regex` and `lazy_static` to be the current latest
- Changed the way that traits are implemented to use macros. Thus reducing
  duplication issues seen previously for each type that wanted to implement
  Inflector
- More tests for conversion between different string formats
- Better documentation for new users
- Cleaned up documentation

## Notes:
- This is a pre-release for 1.0.0

# 0.6.0

## Breaking changes:

- Removed lower and upper case. -- Use the built in [Rust implementations](https://doc.rust-lang.org/std/string/struct.String.html#method.to_uppercase)

## Non-breaking change:

- Removed lib definitions. -- Thanks @kanerogers

# 0.5.1

## Non-breaking change:

- Refactored Title, Pascal, Train and Camel cases to be unified.

# 0.5.0

## New features:

- Adds Train-Case thanks to @stpettersens

## Fixes:

- Fixes performance issues with Title Case

# 0.4.0

## Fixes:

- Fixes issue where strings like `HTTPParty` becomes `h_t_t_p_party` instead of `http_party` as it should -- Thanks @cmsd2

## New features:

- Adds PascalCase

## Benchmarks:
```shell
test cases::camelcase::tests::bench_camel0                      ... bench:         142 ns/iter (+/- 28)
test cases::camelcase::tests::bench_camel1                      ... bench:         143 ns/iter (+/- 30)
test cases::camelcase::tests::bench_camel2                      ... bench:         138 ns/iter (+/- 80)
test cases::camelcase::tests::bench_is_camel                    ... bench:         171 ns/iter (+/- 103)
test cases::classcase::tests::bench_class_case                  ... bench:       2,369 ns/iter (+/- 658)
test cases::classcase::tests::bench_class_from_snake            ... bench:       2,378 ns/iter (+/- 914)
test cases::classcase::tests::bench_is_class                    ... bench:       2,541 ns/iter (+/- 294)
test cases::kebabcase::tests::bench_is_kebab                    ... bench:         180 ns/iter (+/- 35)
test cases::kebabcase::tests::bench_kebab                       ... bench:         156 ns/iter (+/- 91)
test cases::kebabcase::tests::bench_kebab_from_snake            ... bench:         248 ns/iter (+/- 143)
test cases::lowercase::tests::bench_is_lower                    ... bench:         340 ns/iter (+/- 91)
test cases::lowercase::tests::bench_lower                       ... bench:         301 ns/iter (+/- 124)
test cases::pascalcase::tests::bench_is_pascal                  ... bench:         163 ns/iter (+/- 65)
test cases::pascalcase::tests::bench_pascal0                    ... bench:         140 ns/iter (+/- 78)
test cases::pascalcase::tests::bench_pascal1                    ... bench:         140 ns/iter (+/- 40)
test cases::pascalcase::tests::bench_pascal2                    ... bench:         138 ns/iter (+/- 105)
test cases::screamingsnakecase::tests::bench_is_screaming_snake ... bench:         193 ns/iter (+/- 27)
test cases::screamingsnakecase::tests::bench_screaming_snake    ... bench:         161 ns/iter (+/- 84)
test cases::sentencecase::tests::bench_is_sentence              ... bench:         394 ns/iter (+/- 85)
test cases::sentencecase::tests::bench_sentence                 ... bench:         365 ns/iter (+/- 186)
test cases::sentencecase::tests::bench_sentence_from_snake      ... bench:         333 ns/iter (+/- 178)
test cases::snakecase::tests::bench_is_snake                    ... bench:         190 ns/iter (+/- 74)
test cases::snakecase::tests::bench_snake_from_camel            ... bench:         155 ns/iter (+/- 44)
test cases::snakecase::tests::bench_snake_from_snake            ... bench:         280 ns/iter (+/- 161)
test cases::snakecase::tests::bench_snake_from_title            ... bench:         156 ns/iter (+/- 31)
test cases::tablecase::tests::bench_is_table_case               ... bench:       2,388 ns/iter (+/- 431)
test cases::tablecase::tests::bench_table_case                  ... bench:       2,240 ns/iter (+/- 446)
test cases::titlecase::tests::bench_is_title                    ... bench:         786 ns/iter (+/- 135)
test cases::titlecase::tests::bench_title                       ... bench:         826 ns/iter (+/- 278)
test cases::titlecase::tests::bench_title_from_snake            ... bench:         723 ns/iter (+/- 256)
test cases::uppercase::tests::bench_is_upper                    ... bench:         351 ns/iter (+/- 85)
test cases::uppercase::tests::bench_upper                       ... bench:         332 ns/iter (+/- 48)
```

# 0.3.3

## Fixes:

- Fixes issue where camel case tests were not run
- Fixes issue with camel case with numbers

# 0.3.2

## Fixes:

- Fixes issue https://github.com/whatisinternet/inflector/issues/18
- Fixes performance issues overall

## Benchmarks:
```shell
test cases::camelcase::tests::bench_camel0                      ... bench:         139 ns/iter (+/- 40)
test cases::camelcase::tests::bench_camel1                      ... bench:         138 ns/iter (+/- 31)
test cases::camelcase::tests::bench_camel2                      ... bench:         138 ns/iter (+/- 41)
test cases::camelcase::tests::bench_is_camel                    ... bench:         184 ns/iter (+/- 90)
test cases::classcase::tests::bench_class_case                  ... bench:       2,383 ns/iter (+/- 557)
test cases::classcase::tests::bench_class_from_snake            ... bench:       2,393 ns/iter (+/- 1,120)
test cases::classcase::tests::bench_is_class                    ... bench:       2,443 ns/iter (+/- 1,060)
test cases::kebabcase::tests::bench_is_kebab                    ... bench:         182 ns/iter (+/- 60)
test cases::kebabcase::tests::bench_kebab                       ... bench:         161 ns/iter (+/- 98)
test cases::kebabcase::tests::bench_kebab_from_snake            ... bench:         264 ns/iter (+/- 144)
test cases::lowercase::tests::bench_is_lower                    ... bench:         358 ns/iter (+/- 154)
test cases::lowercase::tests::bench_lower                       ... bench:         347 ns/iter (+/- 220)
test cases::screamingsnakecase::tests::bench_is_screaming_snake ... bench:         194 ns/iter (+/- 35)
test cases::screamingsnakecase::tests::bench_screaming_snake    ... bench:         173 ns/iter (+/- 97)
test cases::sentencecase::tests::bench_is_sentence              ... bench:         377 ns/iter (+/- 83)
test cases::sentencecase::tests::bench_sentence                 ... bench:         337 ns/iter (+/- 155)
test cases::sentencecase::tests::bench_sentence_from_snake      ... bench:         370 ns/iter (+/- 176)
test cases::snakecase::tests::bench_is_snake                    ... bench:         191 ns/iter (+/- 98)
test cases::snakecase::tests::bench_snake_from_camel            ... bench:         156 ns/iter (+/- 25)
test cases::snakecase::tests::bench_snake_from_snake            ... bench:         289 ns/iter (+/- 136)
test cases::snakecase::tests::bench_snake_from_title            ... bench:         157 ns/iter (+/- 68)
test cases::tablecase::tests::bench_is_table_case               ... bench:       2,253 ns/iter (+/- 978)
test cases::tablecase::tests::bench_table_case                  ... bench:       2,227 ns/iter (+/- 704)
test cases::titlecase::tests::bench_is_title                    ... bench:         787 ns/iter (+/- 362)
test cases::titlecase::tests::bench_title                       ... bench:         826 ns/iter (+/- 317)
test cases::titlecase::tests::bench_title_from_snake            ... bench:         747 ns/iter (+/- 230)
test cases::uppercase::tests::bench_is_upper                    ... bench:         347 ns/iter (+/- 111)
test cases::uppercase::tests::bench_upper                       ... bench:         328 ns/iter (+/- 42)
```

## OLD Benchmarks:
```shell
test cases::camelcase::tests::bench_camel                                         ... bench:       1,825 ns/iter (+/- 346)
test cases::camelcase::tests::bench_camel_from_sname                              ... bench:       1,223 ns/iter (+/- 289)
test cases::camelcase::tests::bench_is_camel                                      ... bench:      49,416 ns/iter (+/- 593)
test cases::classcase::tests::bench_class_case                                    ... bench: 160,985,376 ns/iter (+/- 5,173,751)
test cases::classcase::tests::bench_class_from_snake                              ... bench: 161,533,425 ns/iter (+/- 4,167,305)
test cases::classcase::tests::bench_is_class                                      ... bench: 161,352,118 ns/iter (+/- 3,793,478)
test cases::kebabcase::tests::bench_is_kebab                                      ... bench:         793 ns/iter (+/- 400)
test cases::kebabcase::tests::bench_kebab                                         ... bench:         752 ns/iter (+/- 310)
test cases::kebabcase::tests::bench_kebab_from_snake                              ... bench:         210 ns/iter (+/- 32)
test cases::lowercase::tests::bench_is_lower                                      ... bench:         340 ns/iter (+/- 86)
test cases::lowercase::tests::bench_lower                                         ... bench:         306 ns/iter (+/- 173)
test cases::screamingsnakecase::tests::bench_is_screaming_snake                   ... bench:         635 ns/iter (+/- 210)
test cases::screamingsnakecase::tests::bench_screaming_snake                      ... bench:         610 ns/iter (+/- 87)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_camel           ... bench:         961 ns/iter (+/- 579)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_class           ... bench:         894 ns/iter (+/- 352)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_kebab           ... bench:         877 ns/iter (+/- 571)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_screaming_snake ... bench:         584 ns/iter (+/- 304)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_sentence        ... bench:       1,123 ns/iter (+/- 630)
test cases::screamingsnakecase::tests::bench_screaming_snake_from_upper_kebab     ... bench:         914 ns/iter (+/- 435)
test cases::sentencecase::tests::bench_is_sentence                                ... bench:       2,714 ns/iter (+/- 796)
test cases::sentencecase::tests::bench_sentence                                   ... bench:       2,678 ns/iter (+/- 1,357)
test cases::sentencecase::tests::bench_sentence_from_snake                        ... bench:       2,100 ns/iter (+/- 1,046)
test cases::snakecase::tests::bench_is_snake                                      ... bench:         626 ns/iter (+/- 191)
test cases::snakecase::tests::bench_snake                                         ... bench:         581 ns/iter (+/- 298)
test cases::snakecase::tests::bench_snake_from_camel                              ... bench:         882 ns/iter (+/- 328)
test cases::snakecase::tests::bench_snake_from_class                              ... bench:         883 ns/iter (+/- 193)
test cases::snakecase::tests::bench_snake_from_kebab                              ... bench:         922 ns/iter (+/- 360)
test cases::snakecase::tests::bench_snake_from_sentence                           ... bench:       1,209 ns/iter (+/- 539)
test cases::snakecase::tests::bench_snake_from_snake                              ... bench:         637 ns/iter (+/- 386)
test cases::snakecase::tests::bench_snake_from_upper_kebab                        ... bench:         876 ns/iter (+/- 488)
test cases::tablecase::tests::bench_is_table_case                                 ... bench:       5,784 ns/iter (+/- 1,129)
test cases::tablecase::tests::bench_table_case                                    ... bench:       5,754 ns/iter (+/- 1,460)
test cases::titlecase::tests::bench_is_title                                      ... bench:       2,847 ns/iter (+/- 1,553)
test cases::titlecase::tests::bench_title                                         ... bench:       2,799 ns/iter (+/- 1,309)
test cases::titlecase::tests::bench_title_from_snake                              ... bench:       2,202 ns/iter (+/- 697)
test cases::uppercase::tests::bench_is_upper                                      ... bench:         357 ns/iter (+/- 55)
test cases::uppercase::tests::bench_upper                                         ... bench:         311 ns/iter (+/- 135)
```

# 0.3.1

## Fixes:

- Fixes issue https://github.com/rust-lang/rust/pull/34660
- Updates Regex to 0.1.73 for latest fixes

# 0.3.0

## Fixes:

- Resolves issues with pluralize not always correctly pluralizing strings.
  Thanks, @weiznich!
- Resolves issues with silently failing test on table case
- Replaces complex code used for is_*_case checks with simple conversion and
  check equality.

## Breaking changes:

- Dropping support for Rust versions below stable

# 0.2.1

## Features:

- Replaced custom implementation of lower and uppercase with Rust default

## Breaking changes:

- Rust 1.2 or greater required

# 0.2.0

## Features:

- Added Pluralize
- Added Singularize
- Added Table case

## Fixes:

- Fixed doc tests to properly run as rust auto wraps doc tests in a function and
  never ran the inner function that was defined.
- Fixed documentation for kebab case
- Fixed several failed tests for various cases which were mainly typos

## Breaking changes:

- Class case now singularizes strings and verifies that strings are
  singularized. Those wishing for the old behaviour should remain on the 0.1.6
  release.


# 0.1.6

## Features:

- Added screaming snake case

# 0.1.5

## Fixes:

- Refactored tests into doc tests.


# 0.1.4

## Features:

- Significant performance improvement for casting strings between different case
  types see #13.

## Fixes:

- Fixed performance issues with string casting.
- Removed heavy reliance on Regex


# 0.1.3

## Fixes:

- Refactored code to mostly pass through snake case then be converted to lower
  the number of moving parts and reduce the complexity for adding a new casting
  as only snake case would need to be modified to convert to most other cases.

## Breaking changes:

- This release is slow in contrast to other crates


# 0.1.2

## Fixes:

- Documentation fixes
- Added uppercase
- Added lowercase
- Added foreign key
- Added sentence case
- Added title case


# 0.1.1

## Features:
- Adds support for foreign key
- Adds demodulize
- Adds deconstantize
- Adds trait based usage


# 0.1.0

## Features:

- Added support for camel case
- Added support for class case
- Added support for kebab case
- Added support for snake case
