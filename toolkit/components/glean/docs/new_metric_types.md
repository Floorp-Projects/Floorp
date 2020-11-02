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

Each metric type gets its own file, mimicking the structure in
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

## C++ and JS

The C++ and JS APIs are implemented [atop the Rust API](code_organization.md).
We treat them both together since, though they're different languages,
they're both implemented in C++ and share much of their implementation.

Each metric type has six pieces you'll need to cover:

### 1. MLA FFI

- Using our convenient macros, define the Multi-Language Architecture's FFI layer above the Rust API in [`api/src/ffi/mod.rs`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/ffi/mod.rs).

### 2. C++ Impl

- Implement a type called `XMetric` (e.g. `CounterMetric`) in `mozilla::glean::impl` in [`bindings/private/`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/private/).
    - Its methods should be named the same as the ones in the Rust API, transformed to `CamelCase`.
    - They should all be public.
- Include the new metric type in [`bindings/MetricTypes.h`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/MetricTypes.h)
- Include the new files in [`moz.build`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/MetricTypes.h). The header file should be added to `EXPORTS.mozilla.glean` and the `.cpp` file should be added to `UNIFIED_SOURCES`.

### 3. IDL

- Duplicate the public API (including its docs) to [`xpcom/nsIGleanMetrics.idl`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/xpcom/nsIGleanMetrics.idl) with the name `nsIGleanX` (e.g. `nsIGleanCounter`).
    - Inherit from `nsISupports`.
    - The naming style for members here is `lowerCamelCase`. You'll need a [GUID](https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Generating_GUIDs) because this is XPCOM, but you'll only need the canonical form since we're only exposing to JS.

### 4. JS Impl

- Add an `nsIGleanX`-deriving, `XMetric`-owning type called `GleanX` (e.g. `GleanCounter`) in the same header and `.cpp` as `XMetric` in [`bindings/private/`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/private/).
    - Don't declare any methods beyond a ctor (takes a `uint32_t` metric id, init-constructs a `impl::XMetric` member) and dtor (`default`): the IDL will do the rest so long as you remember to add `NS_DECL_ISUPPORTS` and `NS_DECL_NSIGLEANX`.
    - In the definition of `GleanX`, member identifiers are back to `CamelCase` and need macros like `NS_IMETHODIMP`. Delegate operations to the owned `XMetric`, returning `NS_OK` no matter what in non-test methods. (Test-only methods can return `NS_ERROR` codes on failures).

### 5. Tests

Two languages means two test suites.

- Add a never-expiring test-only metric of your type to [`test_metrics.yaml`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/test_metrics.yaml).
    - Feel free to be clever with the name, but be sure to make clear that it is test-only.
- **C++ Tests (GTest)** - Add a small test case to [`gtest/TestFog.cpp`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/gtest/TestFog.cpp).
    - For more details, peruse the [testing docs](testing.md).
- **JS Tests (xpcshell)** - Add a small test case to [`xpcshell/test_Glean.js`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/xpcshell/test_Glean.js).
    - For more details, peruse the [testing docs](testing.md).

### 6. API Documentation

TODO
