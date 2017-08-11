# Mio - Metal IO

Mio is a lightweight I/O library for Rust with a focus on adding as little
overhead as possible over the OS abstractions.

[![crates.io](http://meritbadge.herokuapp.com/mio)](https://crates.io/crates/mio)
[![Build Status](https://travis-ci.org/carllerche/mio.svg?branch=master)](https://travis-ci.org/carllerche/mio)
[![Build status](https://ci.appveyor.com/api/projects/status/ok90r1tcgkyndnvw/branch/master?svg=true)](https://ci.appveyor.com/project/carllerche/mio/branch/master)

**API documentation**

* [master](http://carllerche.github.io/mio)
* [v0.6](https://docs.rs/mio/^0.6)
* [v0.5](https://docs.rs/mio/^0.5)

This is a low level library, if you are looking for something easier to get
started with, see [Tokio](https://tokio.rs).

## Usage

To use `mio`, first add this to your `Cargo.toml`:

```toml
[dependencies]
mio = "0.6"
```

Then, add this to your crate root:

```rust
extern crate mio;
```

## Features

* Event loop backed by epoll, kqueue.
* Zero allocations at runtime
* Non-blocking TCP, UDP and Unix domain sockets
* High performance timer system
* Thread safe message channel for cross thread communication

## Non goals

The following are specifically omitted from MIO and are left to the user
or higher level libraries.

* File operations
* Thread pools / multi-threaded event loop

## Platforms

Currently supported platforms:

* Linux
* OS X
* Windows
* NetBSD
* Android
* iOS

There are potentially others. If you find that Mio works on another
platform, submit a PR to update the list!

### Libraries

* [tokio-core](//github.com/tokio-rs/tokio-core) - Underlying event loop
  for the [Tokio project](//github.com/tokio-rs/tokio).
* [mioco](//github.com/dpc/mioco) - Mio COroutines
* [simplesched](//github.com/zonyitoo/simplesched) - Coroutine I/O with a simple scheduler
* [coio-rs](//github.com/zonyitoo/coio-rs) - Coroutine I/O with work-stealing scheduler
* [rotor](//github.com/tailhook/rotor) - A wrapper that allows to create composable I/O libraries on top of mio
* [ws-rs](//github.com/housleyjk/ws-rs) - WebSockets based on Mio

## Community

A group of mio users hang out in the #mio channel on the Mozilla IRC
server (irc.mozilla.org). This can be a good place to go for questions.

## Contributing

Interested in getting involved? We would love to help you! For simple
bug fixes, just submit a PR with the fix and we can discuss the fix
directly in the PR. If the fix is more complex, start with an issue.

If you want to propose an API change, create an issue to start a
discussion with the community. Also, feel free to talk with us in the
IRC channel.

Finally, be kind. We support the [Rust Code of Conduct](https://www.rust-lang.org/conduct.html).
