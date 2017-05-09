# FAQ

A collection of some commonly asked questions, with responses! If you find any
of these unsatisfactory feel free to ping me (@alexcrichton) on github,
acrichto on IRC, or just by email!

### Why both `Item` and `Error` associated types?

An alternative design of the `Future` trait would be to only have one associated
type, `Item`, and then most futures would resolve to `Result<T, E>`. The
intention of futures, the fundamental support for async I/O, typically means
that errors will be encoded in almost all futures anyway though. By encoding an
error type in the future as well we're able to provide convenient combinators
like `and_then` which automatically propagate errors, as well as combinators
like `join` which can act differently depending on whether a future resolves to
an error or not.

### Do futures work with multiple event loops?

Yes! Futures are designed to source events from any location, including multiple
event loops. All of the basic combinators will work on any number of event loops
across any number of threads.

### What if I have CPU intensive work?

The documentation of the `Future::poll` function says that's it's supposed to
"return quickly", what if I have work that doesn't return quickly! In this case
it's intended that this work will run on a dedicated pool of threads intended
for this sort of work, and a future to the returned value is used to represent
its completion.

A proof-of-concept method of doing this is the `futures-cpupool` crate in this
repository, where you can execute work on a thread pool and receive a future to
the value generated. This future is then composable with `and_then`, for
example, to mesh in with the rest of a future's computation.

### How do I call `poll`?

In general it's not recommended to call `poll` unless you're implementing
another `poll` function. If you need to poll a future, however, you can use
`task::spawn` followed by the `poll_future` method on `Spawn<T>`.

### How do I return a future?

Returning a future is like returning an iterator in Rust today. It's not the
easiest thing to do and you frequently need to resort to `Box` with a trait
object. Thankfully though [`impl Trait`] is just around the corner and will
allow returning these types unboxed in the future.

[`impl Trait`]: https://github.com/rust-lang/rust/issues/34511

For now though the cost of boxing shouldn't actually be that high. A future
computation can be constructed *without boxing* and only the final step actually
places a `Box` around the entire future. In that sense you're only paying the
allocation at the very end, not for any of the intermediate futures.

More information can be found [in the tutorial][return-future].

[return-future]: https://github.com/alexcrichton/futures-rs/blob/master/TUTORIAL.md#returning-futures

### Does it work on Windows?

Yes! This library builds on top of mio, which works on Windows.

### What version of Rust should I use?

Rust 1.10 or later.

### Is it on crates.io?

Not yet! A few names are reserved, but crates cannot have dependencies from a
git repository. Right now we depend on the master branch of `mio`, and crates
will be published once that's on crates.io as well!

### Does this implement tail call optimization?

One aspect of many existing futures libraries is whether or not a tail call
optimization is implemented. The exact meaning of this varies from framework to
framework, but it typically boils down to whether common patterns can be
implemented in such a way that prevents blowing the stack if the system is
overloaded for a moment or leaking memory for the entire lifetime of a
future/server.

For the prior case, blowing the stack, this typically arises as loops are often
implemented through recursion with futures. This recursion can end up proceeding
too quickly if the "loop" makes lots of turns very quickly. At this time neither
the `Future` nor `Stream` traits handle tail call optimizations in this case,
but rather combinators are patterns are provided to avoid recursion. For example
a `Stream` implements `fold`, `for_each`, etc. These combinators can often be
used to implement an asynchronous loop to avoid recursion, and they all execute
in constant stack space. Note that we're very interested in exploring more
generalized loop combinators, so PRs are always welcome!

For the latter case, leaking memory, this can happen where a future accidentally
"remembers" all of its previous states when it'll never use them again. This
also can arise through recursion or otherwise manufacturing of futures of
infinite length. Like above, however, these also tend to show up in situations
that would otherwise be expressed with a loop, so the same solutions should
apply there regardless.
