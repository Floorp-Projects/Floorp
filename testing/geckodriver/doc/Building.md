Building geckodriver
====================

geckodriver is written in Rust, a systems programming language
from Mozilla.  Crucially, it relies on the [webdriver crate] to
provide the HTTPD and do most of the heavy lifting of marshalling
the WebDriver protocol. geckodriver translates WebDriver [commands],
[responses], and [errors] to the [Marionette protocol], and acts
as a proxy between [WebDriver] and [Marionette].

geckodriver is an optional build component when building Firefox,
which means it is not built by default when invoking `./mach build`.
To enable building of geckodriver, ensure to put this in your [mozconfig]:

	ac_add_options --enable-geckodriver

Because we use geckodriver in testing, particularly as part of the
Web Platform Tests, it _is_ built by default in the [Firefox CI].
A regular `-b do -p all -u none -t none` try syntax will build
geckodriver on all the supported platforms.  The build will be part
of the `B` task.

If you use artifact builds you may also build geckodriver using cargo:

	% cd testing/geckodriver
	% cargo build
	…
	   Compiling geckodriver v0.21.0 (file:///home/ato/src/gecko/testing/geckodriver)
	…
	    Finished dev [optimized + debuginfo] target(s) in 7.83s

Because all Rust code in central shares the same cargo workspace,
the binary will be put in the `$(topsrcdir)/target` directory.

You can run your freshly built geckodriver this way:

	% ./mach geckodriver -- --other --flags

And run its unit tests like this:

	% ./mach test testing/geckodriver

Or by invoking cargo in its subdirectory:

	% cd testing/geckodriver
	% cargo test

[Rust]: https://www.rust-lang.org/
[webdriver crate]: https://crates.io/crates/webdriver
[commands]: https://docs.rs/webdriver/newest/webdriver/command/
[responses]: https://docs.rs/webdriver/newest/webdriver/response/
[errors]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html
[Marionette protocol]: /testing/marionette/doc/marionette/Protocol.html
[WebDriver]: https://w3c.github.io/webdriver/
[Marionette]: /testing/marionette/doc/marionette
[mozconfig]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options
[Firefox CI]: https://treeherder.mozilla.org/


Making geckodriver part of the official build
---------------------------------------------

There is a long-term intention to include geckodriver as part of
the regular Firefox build.

When we migrated geckodriver from GitHub into central it was originally
enabled by default in all local builds.  Since this was one of the
very first Rust components to land in central, support for Rust was a
little bit experimental and we discovered a couple of different problems:

  (1) Not all developers had Rust installed, and support for Rust
      in the tree was experimental.  This caused some compile
      errors in particular on Windows, and at the time developers
      were not happy with requiring Rust for building Firefox.

  (2) The additional build time induced by including a fairly large
      Rust component in the default build was considered too high.

At the time of writing, Rust support in central has improved vastly.
We also require Rust for regular Firefox builds, which addresses
part of issue 1.

Our working theory is that we now build so much Rust code in central
that many of the dependencies geckodriver relies on and that takes
up a lot of time to build will have been built as dependencies for
other Rust components in the tree, effectively making the additional
time it takes to build geckodriver less prominent than it was when
we originally tried including it in the default build.

This work is tracked as part of
<https://bugzilla.mozilla.org/show_bug.cgi?id=1471281>.
