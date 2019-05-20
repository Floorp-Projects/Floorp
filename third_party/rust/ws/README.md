# WS-RS

Lightweight, event-driven WebSockets for [Rust](https://www.rust-lang.org).
```rust

/// A WebSocket echo server
listen("127.0.0.1:3012", |out| {
    move |msg| {
        out.send(msg)
    }
})
```

Introduction
------------
[![Build Status](https://travis-ci.org/housleyjk/ws-rs.svg?branch=stable)](https://travis-ci.org/housleyjk/ws-rs)
[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)
[![Crate](http://meritbadge.herokuapp.com/ws)](https://crates.io/crates/ws)

**[Homepage](https://ws-rs.org)**

**[API Documentation](https://ws-rs.org/docs)**

This library provides an implementation of WebSockets,
[RFC6455](https://tools.ietf.org/html/rfc6455) using [MIO](https://github.com/carllerche/mio). It
allows for handling multiple connections on a single thread, and even spawning new client
connections on the same thread. This makes for very fast and resource efficient WebSockets. The API
design abstracts away the menial parts of the WebSocket protocol and allows you to focus on
application code without worrying about protocol conformance. However, it is also possible to get
low-level access to individual WebSocket frames if you need to write extensions or want to optimize
around the WebSocket protocol.

Getting Started
---------------

For detailed installation and usage instructions, check out the [guide](https://ws-rs.org/guide).

Features
--------

WS-RS provides a complete implementation of the WebSocket specification. There is also support for
[ssl](https://ws-rs.org/guide/ssl) and
[permessage-deflate](https://ws-rs.org/guide/deflate).

Testing
-------

WS-RS is thoroughly tested and passes the [Autobahn Test Suite](https://crossbar.io/autobahn/) for
WebSockets, including the tests for `permessage-deflate`. Visit
[ws-rs.org](https://ws-rs.org/testing/autobahn/results) to view the results of the latest test run.

Contributing
------------

Please report bugs and make feature requests [here](https://github.com/housleyjk/ws-rs/issues).
