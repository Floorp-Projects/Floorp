# IntlMemoizer

`intl-memoizer` is a crate designed to handle lazy-initialized references
to intl formatters.

The assumption is that allocating a new formatter instance is costly, and such
instance is read-only during its life time, with constructor being expensive, and
`format`/`select` calls being cheap.

In result it pays off to use a singleton to manage memoization of all instances of intl
APIs such as `PluralRules`, DateTimeFormat` etc. between all `FluentBundle` instances.

Usage
-----

```rust
use intl_memoizer::{IntlMemoizer, Memoizable};
use unic_langid::langid;

use intl_pluralrules::{PluralRules, PluralRuleType, PluralCategory};

impl Memoizable for PluralRules {
    type Args = (PluralRulesType,);
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Self {
      Self::new(lang, args.0)
    }
}

fn main() {
    let lang = langid!("en-US");

    // A single memoizer for all languages
    let mut memoizer = IntlMemoizer::new();

    // A RefCell for a particular language to be used in all `FluentBundle`
    // instances.
    let mut en_us_memoizer = memoizer.get_for_lang(lang.clone());

    // Per-call borrow
    let mut en_us_memoizer_borrow = en_us_memoizer.borrow_mut();
    let cb = en_us_memoizer_borrow.get::<PluralRules>((PluralRulesType::Cardinal,));
    assert_eq!(cb.select(1), PluralCategory::One);
}

```

Get Involved
------------

`fluent-rs` is open-source, licensed under the Apache License, Version 2.0.  We
encourage everyone to take a look at our code and we'll listen to your
feedback.


Discuss
-------

We'd love to hear your thoughts on Project Fluent! Whether you're a localizer
looking for a better way to express yourself in your language, or a developer
trying to make your app localizable and multilingual, or a hacker looking for
a project to contribute to, please do get in touch on the mailing list and the
IRC channel.

 - Discourse: https://discourse.mozilla.org/c/fluent
 - IRC channel: [irc://irc.mozilla.org/l20n](irc://irc.mozilla.org/l20n)
