# runloop [![Crates.io](https://img.shields.io/crates/v/runloop.svg)](https://crates.io/crates/runloop) [![Build Status](https://travis-ci.org/ttaubert/rust-runloop.svg?branch=master)](https://travis-ci.org/ttaubert/rust-runloop) [![License](https://img.shields.io/badge/license-MPL2-blue.svg?style=flat)](LICENSE)

This crate provides a cancelable `RunLoop` to simplify writing non-blocking
polling threads.

## Usage

The closure passed to `RunLoop::new()` or `RunLoop::new_with_timeout()` will be
called once and is executed in the newly spawned thread. It should periodically
check, via the given callback argument named `alive` in the below examples,
whether the runloop was requested to terminate.

`RunLoop::alive()` allows the owning thread to check whether the runloop is
still alive. This can be useful for debugging (e.g. assertions) or early
returns on failure in the passed closure.

`RunLoop::cancel()` will request the runloop to terminate. If the runloop is
still active it will join the thread and block. If the runloop already
terminated on its own this will be a no-op.

### Example: An endless runloop

A runloop does not have to terminate on its own, it can wait until the
`cancel()` method is called.

```rust
// This runloop runs until we stop it.
let rloop = RunLoop::new(|alive| {
    while alive() { /* endless loop */ }
})?;

// Loop a few times.
thread::sleep_ms(500);
assert!(rloop.alive());

// This will cause `alive()` to return false
// and block until the thread was joined.
rloop.cancel();
```

### Example: A runloop with a timeout

Creating a runloop via `new_with_timeout()` ensures that `alive()` returns
`false` after the given duration.

```rust
// This runloop will time out after 10ms.
let rloop = RunLoop::new_with_timeout(|alive| {
    while alive() { /* endless loop */ }
}, 10)?;

// Wait for the timeout.
while rloop.alive() { /* loop */ }

// This won't block anymore, it's a no-op.
// The runloop has already terminated.
rloop.cancel();
```

### Example: A runloop with channels

As a runloop will run the given closure in a newly spawned thread it requires
channels and similar utilities to communicate back to the owning thread.

```rust
let (tx, rx) = channel();

// This runloop will send a lot of numbers.
let rloop = RunLoop::new(move |alive| {
    let mut counter = 0u64;
    while alive() {
        tx.send(counter).unwrap();
        counter += 1;
    }
})?;

// Wait until we receive a specific number.
while rx.recv().unwrap() < 50 { /* loop */ }

// We've seen enough.
rloop.cancel();
```

## License

`runloop` is distributed under the terms of the Mozilla Public License, v. 2.0.

See [LICENSE](LICENSE) for details.
