bits\_client
============

Interfaces for BITS.

bits\_client lib
---------------

`bits_client` is the primary target and provides `BitsClient`, an API for creating and monitoring BITS jobs.

`bits_client::new()` creates a `BitsClient` that does all operations within the current process, as the current user.

bits crate
----------

`bits` is a safe interface to BITS, providing connections to the
Background Copy Manager, some basic operations on Background Copy Jobs, and
methods for implementing `IBackgroundCopyCallback`s in Rust.

test\_client example
-------------------

`examples/test_client.rs` shows how to use the API.
