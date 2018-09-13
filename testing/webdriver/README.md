webdriver library
=================

The [webdriver crate] is a library implementation of the wire protocol
for the [W3C WebDriver standard] written in Rust.  WebDriver is a remote
control interface that enables introspection and control of user agents.
It provides a platform- and language-neutral wire protocol as a way
for out-of-process programs to remotely instruct the behaviour of web
browsers.

The webdriver library provides the formal types, error codes, type and
bounds checks, and JSON marshaling conventions for correctly parsing
and emitting the WebDriver protocol.  It also provides an HTTP server
where endpoints are mapped to the different WebDriver commands.

**As of right now, this is an implementation for the server side of the
WebDriver API in Rust, not the client side.**

[webdriver crate]: https://crates.io/crates/webdriver
[W3C WebDriver standard]: https://w3c.github.io/webdriver/


Building
========

The library is built using the usual [Rust conventions]:

	% cargo build

To run the tests:

	% cargo test

[Rust conventions]: http://doc.crates.io/guide.html


Contact
=======

The mailing list for webdriver discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

There is also an IRC channel to talk about using and developing
webdriver in #ateam on irc.mozilla.org.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://lists.mozilla.org/pipermail/tools-marionette/
