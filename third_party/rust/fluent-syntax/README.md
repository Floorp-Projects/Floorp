# Fluent Syntax

`fluent-syntax` is a parser/serializer API for the Fluent Syntax, part of the [Project Fluent](https://projectfluent.org/), a localization
framework designed to unleash the entire expressive power of natural language translations.

[![crates.io](https://meritbadge.herokuapp.com/fluent-syntax)](https://crates.io/crates/fluent-syntax)
[![Build and test](https://github.com/projectfluent/fluent-rs/workflows/Build%20and%20test/badge.svg)](https://github.com/projectfluent/fluent-rs/actions?query=branch%3Amaster+workflow%3A%22Build+and+test%22)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=master)](https://coveralls.io/github/projectfluent/fluent-rs?branch=master)

Status
------

The crate currently provides just a parser, which is tracking Fluent Syntax on its way to 1.0.

Local Development
-----------------

    cargo build
    cargo test
    cargo bench

When submitting a PR please use  [`cargo fmt`][] (nightly).

[`cargo fmt`]: https://github.com/rust-lang-nursery/rustfmt


Learn the FTL syntax
--------------------

FTL is a localization file format used for describing translation resources.
FTL stands for _Fluent Translation List_.

FTL is designed to be simple to read, but at the same time allows to represent
complex concepts from natural languages like gender, plurals, conjugations, and
others.

    hello-user = Hello, { $username }!

[Read the Fluent Syntax Guide][] in order to learn more about the syntax.  If
you're a tool author you may be interested in the formal [EBNF grammar][].

[Read the Fluent Syntax Guide]: http://projectfluent.org/fluent/guide/
[EBNF grammar]: https://github.com/projectfluent/fluent/tree/master/spec


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
a project to contribute to, please do get in touch on discourse and the IRC channel.

 - Discourse: https://discourse.mozilla.org/c/fluent
 - IRC channel: [irc://irc.mozilla.org/l20n](irc://irc.mozilla.org/l20n)
