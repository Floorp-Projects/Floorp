# INTL Plural Rules

`intl_pluralrules` categorizes numbers by plural operands. See [Unicode Plural Rules](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules)


[![crates.io](https://img.shields.io/crates/v/intl_pluralrules.svg)](https://crates.io/crates/intl_pluralrules)
[![Build Status](https://travis-ci.org/zbraniecki/pluralrules.svg?branch=master)](https://travis-ci.org/zbraniecki/pluralrules)
[![Coverage Status](https://coveralls.io/repos/github/zbraniecki/pluralrules/badge.svg?branch=master)](https://coveralls.io/github/zbraniecki/pluralrules?branch=master)

This library is intended to be used to find the plural category of numeric input.

Status
------

Currently produces operands compliant with CLDR 36 into Rust 1.31 and above.

**Updating CLDR Data**

If you would like to update rules.rs with plural rules that are not the specified version above (e.g. future versions of CLDR or external CLDR-compliant rules), you can regenerate the logic in rules.rs with the command:

	cargo regenerate-data

You will need to replace the JSON files under `/cldr_data/` with your new CLDR JSON files.

Local Development
-----------------

    cargo build
    cargo test

When submitting a PR please use  `cargo fmt`.

Contributors
------------

* [manishearth](https://github.com/manishearth)

Thank you to all contributors!

[CLDR]: https://cldr.unicode.org/
[PluralRules]: https://cldr.unicode.org/index/cldr-spec/plural-rules
[LDML Language Plural Rules Syntax]: https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules
