Atom
====

[![Build Status](https://travis-ci.org/slide-rs/atom.svg?branch=master)](https://travis-ci.org/csherratt/atom)
[![Atom](http://meritbadge.herokuapp.com/atom)](https://crates.io/crates/atom)

`Atom` is a simple abstraction around Rust's `AtomicPtr`. It provides a simple, wait-free way to exchange
data between threads safely. `Atom` is built around the principle that an atomic swap can be used to
safely emulate Rust's ownership.

![store](https://raw.githubusercontent.com/csherratt/atom/master/.store.png)

Using [`store`](https://doc.rust-lang.org/std/sync/atomic/struct.AtomicPtr.html#method.store) to set a shared
atomic pointer is unsafe in rust (or any language) because the contents of the pointer can be overwritten at any
point in time causing the contents of the pointer to be lost. This can cause your system to leak memory, and
if you are expecting that memory to do something useful (like wake a sleeping thread), you are in trouble.

![load](https://raw.githubusercontent.com/csherratt/atom/master/.load.png)

Similarly, [`load`](https://doc.rust-lang.org/std/sync/atomic/struct.AtomicPtr.html#method.store) 
is unsafe since there is no guarantee that that pointer will live for even a cycle after you have read it. Another
thread may modify the pointer, or free it. For `load` to be safe you need to have some outside contract to preserve
the correct ownership semantics.

![swap](https://raw.githubusercontent.com/csherratt/atom/master/.swap.png)

A [`swap`](https://doc.rust-lang.org/std/sync/atomic/struct.AtomicPtr.html#method.swap) is special as it allows
a reference to be exchanged without the risk of that pointer being freed, or stomped on. When a thread
swaps an `AtomicPtr` the old pointer ownership is moved to the caller, and the `AtomicPtr` takes ownership of the new
pointer.


Using `Atom`
------------

Add atom your `Cargo.toml`
```
[dependencies]
atom="*"
```

A short example:
```rust
extern crate atom;

use std::sync::Arc;
use std::thread;
use atom::*;

fn main() {
    // Create an empty atom
    let shared_atom = Arc::new(Atom::empty());

    // set the value 75 
    shared_atom.swap(Box::new(75));

    // Spawn a bunch of thread that will try and take the value
    let threads: Vec<thread::JoinHandle<()>> = (0..8).map(|_| {
        let shared_atom = shared_atom.clone();
        thread::spawn(move || {
            // Take the contents of the atom, only one will win the race
            if let Some(v) = shared_atom.take() {
                println!("I got it: {:?} :D", v);
            } else {
                println!("I did not get it :(");
            }
        })
    }).collect();

    // join the threads
    for t in threads { t.join().unwrap(); }

```

The result will look something like this:
```
I did not get it :(
I got it: 75 :D
I did not get it :(
I did not get it :(
I did not get it :(
I did not get it :(
I did not get it :(
I did not get it :(
```

Using an `Atom` has some advantages over using a raw `AtomicPtr`. First, you don't need any
unsafe code in order to convert the `Box<T>` to and from a `Box` the library handles that for
you. Secondly, `Atom` implements `drop` so you won't accidentally leak a pointer when dropping
your data structure.

AtomSetOnce
-----------

This is an additional bit of abstraction around an Atom. Recall that I said `load` was unsafe
unless you have an additional restrictions. `AtomSetOnce` as the name indicates may only be
set once, and then it may never be unset. We know that if the `Atom` is set the pointer will be
valid for the lifetime of the `Atom`. This means we can implement `Deref` in a safe way.

Take a look at the `fifo` example to see how this can be used to write a lock-free linked list.

