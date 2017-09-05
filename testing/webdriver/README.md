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
[W3C WebDriver standard]: https://w3c.github.io/webdriver/webdriver-spec.html


Building
========

The library is built using the usual [Rust conventions]:

    % cargo build

To run the tests:

    % cargo test

[Rust conventions]: http://doc.crates.io/guide.html


Usage
=====

To start an HTTP server that handles incoming command requests, a request
handler needs to be implemented.  It takes an incoming `WebDriverMessage`
and emits a `WebDriverResponse`:

	impl WebDriverHandler for MyHandler {
	    fn handle_command(
	        &mut self,
	        _: &Option<Session>,
	        msg: WebDriverMessage,
	    ) -> WebDriverResult<WebDriverResponse> {
	        …
	    }

	    fn delete_session(&mut self, _: &Option<Session>) {
	        …
	    }
	}

	let addr = SocketAddr::new("localhost", 4444);
	let handler = MyHandler {};
	let server = webdriver::server::start(addr, handler, vec![])?;
	info!("Listening on {}", server.socket);

It is also possible to provide so called [extension commands] by providing
a vector of known extension routes, for which each new route needs to
implement the `WebDriverExtensionRoute` trait.  Each route needs to map
to a `WebDriverExtensionCommand`:

	pub enum MyExtensionRoute { HelloWorld }
	pub enum MyExtensionCommand { HelloWorld }

	impl WebDriverExtensionRoute for MyExtensionRoute {
	    fn command(
	        &self,
	        captures: &Captures,
	        body: &Json,
	    ) -> WebDriverResult<WebDriverCommand<MyExtensionCommand>> {
	        …
	    }
	}

	let extension_routes = vec![
	    (Method::Get, "/session/{sessionId}/moz/hello", MyExtensions::HelloWorld)
	];

	…

	let server = webdriver::server::start(addr, handler, extension_routes[..])?;

[extension commands]: https://w3c.github.io/webdriver/webdriver-spec.html#dfn-extension-commands

Contact
=======

The mailing list for webdriver discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

There is also an IRC channel to talk about using and developing webdriver
in #ateam on irc.mozilla.org.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: http://groups.google.com/group/mozilla.tools.marionette
