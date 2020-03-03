Building geckodriver
====================

geckodriver is written in [Rust], a systems programming language
from Mozilla.  Crucially, it relies on the [webdriver crate] to
provide the HTTPD and do most of the heavy lifting of marshalling
the WebDriver protocol. geckodriver translates WebDriver [commands],
[responses], and [errors] to the [Marionette protocol], and acts
as a proxy between [WebDriver] and [Marionette].

To build geckodriver:

	% ./mach build testing/geckodriver

If you use artifact builds you may build geckodriver using cargo,
since mach in this case does not have a compile environment:

	% cd testing/geckodriver
	% cargo build
	…
	   Compiling geckodriver v0.21.0 (file:///code/gecko/testing/geckodriver)
	…
	    Finished dev [optimized + debuginfo] target(s) in 7.83s

Because all Rust code in central shares the same cargo workspace,
the binary will be put in the `$(topsrcdir)/target` directory.

You can run your freshly built geckodriver this way:

	% ./mach geckodriver -- --other --flags

See [Testing](Testing.html) for how to run tests.

[Rust]: https://www.rust-lang.org/
[webdriver crate]: https://crates.io/crates/webdriver
[commands]: https://docs.rs/webdriver/newest/webdriver/command/
[responses]: https://docs.rs/webdriver/newest/webdriver/response/
[errors]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html
[Marionette protocol]: /testing/marionette/doc/marionette/Protocol.html
[WebDriver]: https://w3c.github.io/webdriver/
[Marionette]: /testing/marionette/doc/marionette
