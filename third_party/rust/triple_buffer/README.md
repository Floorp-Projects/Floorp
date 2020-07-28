# Triple buffering in Rust

[![On crates.io](https://img.shields.io/crates/v/triple_buffer.svg)](https://crates.io/crates/triple_buffer)
[![On docs.rs](https://docs.rs/triple_buffer/badge.svg)](https://docs.rs/triple_buffer/)
[![Continuous Integration](https://github.com/HadrienG2/triple-buffer/workflows/Continuous%20Integration/badge.svg)](https://github.com/HadrienG2/triple-buffer/actions?query=workflow%3A%22Continuous+Integration%22)
![Requires rustc 1.34+](https://img.shields.io/badge/rustc-1.34+-red.svg)


## What is this?

This is an implementation of triple buffering written in Rust. You may find it
useful for the following class of thread synchronization problems:

- There is one producer thread and one consumer thread
- The producer wants to update a shared memory value periodically
- The consumer wants to access the latest update from the producer at any time

The simplest way to use it is as follows:

```rust
// Create a triple buffer:
let buf = TripleBuffer::new(0);

// Split it into an input and output interface, to be respectively sent to
// the producer thread and the consumer thread:
let (mut buf_input, mut buf_output) = buf.split();

// The producer can move a value into the buffer at any time
buf_input.write(42);

// The consumer can access the latest value from the producer at any time
let latest_value_ref = buf_output.read();
assert_eq!(*latest_value_ref, 42);
```

In situations where moving the original value away and being unable to
modify it after the fact is too costly, such as if creating a new value
involves dynamic memory allocation, you can opt into the lower-level "raw"
interface, which allows you to access the buffer's data in place and
precisely control when updates are propagated.

This data access method is more error-prone and comes at a small performance
cost, which is why you will need to enable it explicitly using the "raw"
[cargo feature](http://doc.crates.io/manifest.html#usage-in-end-products).

```rust
// Create and split a triple buffer
use triple_buffer::TripleBuffer;
let buf = TripleBuffer::new(String::with_capacity(42));
let (mut buf_input, mut buf_output) = buf.split();

// Mutate the input buffer in place
{
    // Acquire a reference to the input buffer
    let raw_input = buf_input.raw_input_buffer();

    // In general, you don't know what's inside of the buffer, so you should
    // always reset the value before use (this is a type-specific process).
    raw_input.clear();

    // Perform an in-place update
    raw_input.push_str("Hello, ");
}

// Publish the input buffer update
buf_input.raw_publish();

// Manually fetch the buffer update from the consumer interface
buf_output.raw_update();

// Acquire a mutable reference to the output buffer
let raw_output = buf_output.raw_output_buffer();

// Post-process the output value before use
raw_output.push_str("world!");
```


## Give me details! How does it compare to alternatives?

Compared to a mutex:

- Only works in single-producer, single-consumer scenarios
- Is nonblocking, and more precisely bounded wait-free. Concurrent accesses will
  be slowed down by cache contention, but no deadlock, livelock, or thread
  scheduling induced slowdown is possible.
- Allows the producer and consumer to work simultaneously
- Uses a lot more memory (3x payload + 3x bytes vs 1x payload + 1 bool)
- Does not allow in-place updates, as the producer and consumer do not access
  the same memory location
- Should be slower if updates are rare and in-place updates are much more
  efficient than moves, faster otherwise.

Compared to the read-copy-update (RCU) primitive from the Linux kernel:

- Only works in single-producer, single-consumer scenarios
- Has higher dirty read overhead on relaxed-memory architectures (ARM, POWER...)
- Does not require accounting for reader "grace periods": once the reader has
  gotten access to the latest value, the synchronization transaction is over
- Does not use the inefficient compare-and-swap hardware primitive on update
- Does not suffer from the ABA problem, allowing much simpler code
- Allocates memory on initialization only, rather than on every update
- May use more memory (3x payload + 3x bytes vs 1x pointer + amount of
  payloads and refcounts that depends on the readout and update pattern)
- Should be slower if updates are rare, faster if updates are frequent

Compared to sending the updates on a message queue:

- Only works in single-producer, single-consumer scenarios (queues can work in
  other scenarios, although the implementations are much less efficient)
- Consumer only has access to the latest state, not the previous ones
- Consumer does not *need* to get through every previous state
- Is nonblocking AND uses bounded amounts of memory (with queues, it's a choice)
- Can transmit information in a single move, rather than two
- Should be faster for any compatible use case

In short, triple buffering is what you're after in scenarios where a shared
memory location is updated frequently by a single writer, read by a single
reader who only wants the latest version, and you can spare some RAM.

- If you need multiple producers, look somewhere else
- If you need multiple consumers, you may be interested in my related "SPMC
  buffer" work, which basically extends triple buffering to multiple consumers
- If you can't tolerate the RAM overhead or want to update the data in place,
  try a Mutex instead (or possibly an RWLock)
- If the shared value is updated very rarely (e.g. every second), try an RCU
- If the consumer must get every update, try a message queue


## How do I know your unsafe lock-free code is working?

By running the tests, of course! Which is unfortunately currently harder than
I'd like it to be.

First of all, we have sequential tests, which are very thorough but obviously
do not check the lock-free/synchronization part. You run them as follows:

    $ cargo test && cargo test --features raw

Then we have a concurrent test where a reader thread continuously observes the
values from a rate-limited writer thread, and makes sure that he can see every
single update without any incorrect value slipping in the middle.

This test is more important, but it is also harder to run because one must first
check some assumptions:

- The testing host must have at least 2 physical CPU cores to test all possible
  race conditions
- No other code should be eating CPU in the background. Including other tests.
- As the proper writing rate is system-dependent, what is configured in this
  test may not be appropriate for your machine.

Taking this and the relatively long run time (~10-20 s) into account, this test
is ignored by default.

Finally, we have benchmarks, which allow you to test how well the code is
performing on your machine. Because cargo bench has not yet landed in Stable
Rust, these benchmarks masquerade as tests, which make them a bit unpleasant to
run. I apologize for the inconvenience.

To run the concurrent test and the benchmarks, make sure no one is eating CPU in
the background and do:

    $ cargo test --release -- --ignored --nocapture --test-threads=1

(As before, you can also test with `--features raw`)

Here is a guide to interpreting the benchmark results:

* `clean_read` measures the triple buffer readout time when the data has not
  changed. It should be extremely fast (a couple of CPU clock cycles).
* `write` measures the amount of time it takes to write data in the triple
  buffer when no one is reading.
* `write_and_dirty_read` performs a write as before, immediately followed by a
  sequential read. To get the dirty read performance, substract the write time
  from that result. Writes and dirty read should take comparable time.
* `concurrent_write` measures the write performance when a reader is
  continuously reading. Expect significantly worse performance: lock-free
  techniques can help against contention, but are not a panacea.
* `concurrent_read` measures the read performance when a writer is continuously
  writing. Again, a significant hit is to be expected.

On an Intel Xeon E5-1620 v3 @ 3.50GHz, typical results are as follows:

* Write: 7.8 ns
* Clean read: 1.8 ns
* Dirty read: 9.3 ns
* Concurrent write: 45 ns
* Concurrent read: 126 ns


## License

This crate is distributed under the terms of the MPLv2 license. See the LICENSE
file for details.

More relaxed licensing (Apache, MIT, BSD...) may also be negociated, in
exchange of a financial contribution. Contact me for details at 
knights_of_ni AT gmx DOTCOM.
