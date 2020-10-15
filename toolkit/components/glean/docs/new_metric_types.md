# Adding a New Metric Type

This document covers how to add a new metric type to FOG.
You should only have to do this if a new metric type is added to the
[Glean SDK](https://mozilla.github.io/glean/book/user/metrics/index.html)
and it is needed in Firefox Desktop.

## IPC

For detailed information about the IPC design,
including a list of forbidden operations,
please consult
[the FOG IPC documentation](ipc.md).

When adding a new metric type, the main IPC considerations are:
* Which operations are forbidden because they are not commutative?
    * Most `set`-style operations cannot be reconciled sensibly across multiple processes.
* If there are non-forbidden operations,
what partial representation will this metric have in non-main processes?
Put another way, what shape of storage will this take up in the
[IPC Payload](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/ipc.rs)?
    * For example, Counters can aggregate all partial counts together to a single
    "partial sum". So
    [its representation](https://searchfox.org/mozilla-central/rev/803b368879fa332e8e2c1840bf1ec164f7ed2c32/toolkit/components/glean/api/src/ipc.rs#45)
    in the IPC Payload is just a single number per Counter.
    * In contrast, Timing Distributions' durations are calculated in the core,
    so it can't calculate samples in child processes.
    Instead we will need to record the start and stop instants as pairs,
    more or less sending a command stream with timing information.

To implement IPC support in a metric type,
we split the metric into three pieces:
1. An umbrella `enum` with the name `MetricTypeMetric`.
    * It has a `Child` and a `Parent` variant.
    * It is IPC-aware and is responsible for
        * If on a non-parent-process,
        either storing partial representations in the IPC Payload,
        or logging errors if forbidden non-test APIs are called.
        (Or panicking if test APIs are called.)
        * If on the parent process, dispatching API calls to the core.
2. The `MetricTypeImpl` is the parent-process implementation.
    * It talks to the core and supports test APIs too.
3. The `MetricTypeIpc` is the ipc-aware non-parent-process implementation.
    * It stores the `MetricId` that identifies this particular metric in a cross-process fashion.

**Note:** This will change once the Rust Language Binding is moved into
[mozilla/glean](https://github.com/mozilla/glean/)
in
[bug 1662868](https://bugzilla.mozilla.org/show_bug.cgi?id=1662868).

## Rust

FOG is being used to test out the ergonomics of a public Rust API,
so has been granted special permission to implement directly atop `glean_core`.
For now.

The Rust API is implemented in the
[`private` module of the `fog` crate](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/metrics).

Each metric gets its own file, mimicking the structure in
[`glean_core`](https://github.com/mozilla/glean/tree/master/glean-core/src/metrics).

Every method on the metric type is public for now,
including test methods.

To allow for pre-init instrumentation to work
(and to be sensitive to performance in the event that metric recording is expensive)
each write to a metric is dispatched to another thread using the
[`Dispatcher`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/dispatcher/mod.rs).
Test methods must first call
`dispatcher::block_on_queue()`
before retrieving stored data to ensure pending operations have been completed before we read the value.

## C++

To Be Implemented.

## JS

To Be Implemented.
