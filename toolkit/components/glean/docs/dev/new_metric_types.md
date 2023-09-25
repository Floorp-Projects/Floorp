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
    * In contrast, Timing Distributions' bucket arrangements are known only to the core,
    so it can't combine sample counts in child processes.
    Instead we record durations in the highest resolution (nanos),
    and send a stream of high-precision samples across IPC.

To implement IPC support in a metric type,
we split FOG's Rust implementation of the metric into three pieces:
1. An umbrella `enum` with the name `MetricTypeMetric`.
    * It has a `Child` and a `Parent` variant.
    * It is IPC-aware and is responsible for
        * If on a non-parent-process,
        either storing partial representations in the IPC Payload,
        or logging errors if forbidden non-test APIs are called.
        (Or panicking if test APIs are called.)
        * If on the parent process, dispatching API calls on its inner Rust Language Binding metric.
2. The parent-process implementation is supplied by
   [the RLB](https://crates.io/crates/glean/).
    * For testing, it stores the `MetricId` that identifies this particular metric in a cross-process fashion.
    * For testing, it exposes a `child_metric()` function to create its `Child` equivalent.
    * For testing and if it supports operations in a non-parent-process,
      it exposes a `metric_id()` function to access the stored `MetricId`.
3. The `MetricTypeIpc` is the non-parent-process implementation.
    * If it does support operations in non-parent processes it stores the
      `MetricId` that identifies this particular metric in a cross-process fashion.

## Mirrors

FOG can mirror Glean metrics to Telemetry probes via the
[Glean Interface For Firefox Telemetry](../user/gifft.md).

Can this metric type be mirrored?
Should it be mirrored?

If so, add an appropriate Telemetry probe for it to mirror to,
documenting the compatibility in
[the GIFFT docs](../user/gifft.md).

### GIFFT Tests

If you add a GIFFT mirror, don't forget to test that the mirror works.
You should be able to do this by adding a task to
[`toolkit/components/glean/tests/xpcshell/test_GIFFT.js`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/tests/xpcshell/test_GIFFT.js).

### GIFFT C++ State: Typical Locking and Shutdown

Some metric types (`labeled_*`, `timespan`, `timing_distribution`)
require holding state in C++ to make GIFFT work.
Pings also hold state to support `testBeforeNextSubmit()`.
If your new metric type requires state in C++,
the current state-of-the-art is a `StaticDataMutex`-locked `UniquePtr` to a `nsTHashTable`.
Access to the inner map is guarded by the lock and is controlled and lazily-instantiated through a single access function.
[See Ping's `GetCallbackMapLock()`](https://searchfox.org/mozilla-central/source/toolkit/components/glean/bindings/private/Ping.cpp)
for example.

It is important to clear this state to avoid leaks.
(See [bug 1752417](https://bugzilla.mozilla.org/show_bug.cgi?id=1752417).)
However, instrumentation may call metrics APIs at any time.

Therefore, GIFFT explicitly stops supporting these state-requiring operations after the
[`AppShutdownTelemetry` shutdown phase](https://searchfox.org/mozilla-central/source/xpcom/base/ShutdownPhase.h).
This is because during the next phase (`XPCOMWillShutdown`) we clear the state.

## Rust

FOG uses the Rust Language Binding APIs (the `glean` crate) with a layer of IPC on top.

The IPC additions and glean-core trait implementations are in the
[`private` module of the `fog` crate](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/metrics).

Each metric type gets its own file, mimicking the structure in
[`glean_core`](https://github.com/mozilla/glean/tree/main/glean-core/src/metrics)
and [`glean`](https://github.com/mozilla/glean/tree/main/glean-core/rlb/src/private).
Unless, of course, that metric is a labeled metric type.
Then the sub metric type gets its own file,
and you need to add "Labeledness" to it by implementing
`Sealed` for your new type following the pattern in
[`api/src/private/labeled.rs`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/private/labeled.rs).

Every method on the metric type is public for now,
including test methods,
and is at least all the methods exposed via the
[metric traits](https://github.com/mozilla/glean/tree/main/glean-core/src/traits).

To support IPC and the MLA FFI (see below)
we identify metric instances by MetricId and store them in maps in
[the `__glean_metric_maps` mod of `metrics.rs`](https://hg.mozilla.org/mozilla-central/toolkit/components/glean/api/src/metrics.rs).
This work is done by the `rust.py` and `rust(_pings).jinja2` extensions to `glean_parser` found
[in the `build_scripts/glean_parser_ext/` folder](https://searchfox.org/mozilla-central/source/toolkit/components/glean/build_scripts/glean_parser_ext).

You shouldn't have to edit these files for new metric types,
as the original modifications to `glean_parser` for this type should already be generating correct code.

### Rust Tests

You should be able to smoke test the basic functionality in Rust unit tests.
You can do this within the metric type implementation file directly.

## C++ and JS

The C++ and JS APIs are implemented [atop the Rust API](code_organization.md).
We treat them both together since, though they're different languages,
they're both implemented in C++ and share much of their implementation.

The overall design is to build the C++ API atop the Multi-Language Architecture's
(MLA's) FFI, then build the JS API atop the C++ API.
This allows features like the
[Glean Interface For Firefox Telemetry (GIFFT)](../user/gifft.md)
that target only C++ and JS to be more simply implemented in the C++ layer.
Exceptions to this (where the JS uses the FFI directly) are discouraged.

Each metric type has six pieces you'll need to cover:

### 1. MLA FFI

- Using our convenient macros,
  define the metric type's Multi-Language Architecture FFI layer above the Rust API in
  [`api/src/ffi/`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/api/src/ffi/).

### 2. C++ Impl

- Implement a type called `XMetric` (e.g. `CounterMetric`) in `mozilla::glean::impl` in
  [`bindings/private/`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/private/).
    - Its methods should be named the same as the ones in the Rust API, transformed to `CamelCase`.
    - They should all be public.
    - Multiplex the FFI's `test_have` and `test_get` functions into a single
      `TestGetValue` function that returns a
      `mozilla::Maybe` wrapping the C++ type that best fits the metric type.
- Include the new metric type in
  [`bindings/MetricTypes.h`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/MetricTypes.h).
- Include the new files in
  [`moz.build`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/moz.build).
  The header file should be added to `EXPORTS.mozilla.glean.bindings` and the
  `.cpp` file should be added to `UNIFIED_SOURCES`.

### 3. IDL

- Duplicate the public API (including its docs) to
  {searchfox}`dom/webidl/GleanMetrics.webidl`
  with the name `GleanX` (e.g. `GleanCounter`).
    - Inherit from `GleanMetric`.
    - The naming style for methods here is `lowerCamelCase`.
    - If the metric method is a reserved word, prepend it with a `_`.
    - Web IDL bindings use
      [their own mapping for types](/dom/webIdlBindings/index.md).
      If you choose ones that most closely resemble the C++ types,
      you'll make your life easier.
- Add a new mapping in `dom/bindings/Bindings.conf`:
  ```idl
  'GleanX': {
      'nativeType': 'mozilla::glean::GleanX',
      'headerFile': 'mozilla/glean/bindings/X.h',
  },
  ```
    - If you don't, you will get a build error complaining `fatal error: 'mozilla/dom/GleanX.h' file not found`.

### 4. JS Impl

- Implement the `GleanX` (e.g. `GleanCounter`) type
  in the same header and `.cpp` as `XMetric` in
  {searchfox}`toolkit/components/glean/bindings/private/`
    - It should own an instance of and delegate method implementations to `XMetric`.
    - In the definition of `GleanX`, member identifiers are back to
      `CamelCase`.
    - Test-only methods can throw `DataError` on failure.
    - Review the [Web IDL Bindings documentation](/dom/webIdlBindings/index.html)
      for help with optional, nullable, and non-primitive types.

### 6. Tests

Two languages means two test suites.

- Add a never-expiring test-only metric of your type to
  [`test_metrics.yaml`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/tests/test_metrics.yaml).
    - Feel free to be clever with the name,
      but be sure to make clear that it is test-only.
- **C++ Tests (GTest)** - Add a small test case to
  [`gtest/TestFog.cpp`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/tests/gtest/TestFog.cpp).
    - For more details, peruse the [testing docs](testing.md).
- **JS Tests (xpcshell)** - Add a small test case to
  [`xpcshell/test_Glean.js`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/tests/xpcshell/test_Glean.js)
  and
  [`xpcshell/test_JOG.js`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/tests/xpcshell/test_JOG.js).
  If your metric type has supported IPC operations, also add cases to the `IPC` variants of these test files.
    - For more details, peruse the [testing docs](testing.md).

### 7. API Documentation

Metric API Documentation is centralized in
[the Glean SDK Book](https://mozilla.github.io/glean/book/user/metrics/index.html).

You will need to craft a Pull Request against
[the SDK](https://github.com/mozilla/glean/)
adding a C++ and JS example to the specific metric type's API docs.

Add a notice at the top of both examples that these APIs are only available in Firefox Desktop:
````md
<div data-lang="C++" class="tab">

> **Note**: C++ APIs are only available in Firefox Desktop.

```c++
#include "mozilla/glean/GleanMetrics.h"

mozilla::glean::category_name::metric_name.Api(args);
```

There are test APIs available too:

```c++
#include "mozilla/glean/GleanMetrics.h"

ASSERT_EQ(value, mozilla::glean::category_name::metric_name.TestGetValue().ref());
```
</div>

// and again for <div data-lang="JS">
````

If you're lucky, the Rust API will have already been added.
Otherwise you'll need to write an example for that one too.

### 8. Labeled metrics (if necessary)

If your new metric type is Labeled, you have more work to do.
I'm assuming you've already implemented the non-labeled sub metric type following the steps above.
Now you must add "Labeledness" to it.

There are four pieces to this:

#### FFI

- To add the writeable storage Rust will use to store the dynamically-generated sub metric instances,
  add your sub metric type's map as a list item in the `submetric_maps` `mod` of
  [`rust.jinja2`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/build_scripts/glean_parser_ext/templates/rust.jinja2).
- Following the pattern of the others, add a `fog_{your labeled metric name here}_get()` FFI API to
  `api/src/ffi/mod.rs`.
  This is what C++ and JS will use to allocate and retrieve sub metric instances by id.

#### C++

- Following the pattern of the others, add a template specialiation for `Labeled<YourSubMetric>::Get` to
  [`bindings/private/Labeled.cpp`](https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/glean/bindings/private/Labeled.cpp).
  This will ensure C++ consumers can fetch or create sub metric instances.

#### JS

- Already handled for you since the JS types all inherit from `GleanMetric`
  and the JS template knows to add your new type to `NewSubMetricFromIds(...)`
  (see `GleanLabeled::NamedGetter` if you're curious).

#### Tests

- The labeled variant will need tests the same as Step #6.
  A tip: be sure to test two labels with different values.

## Python Tests

We have a suite of tests for ensuring code generation generates appropriate code.
You should add a metric to [that suite](testing.md) for your new metric type.
You will need to regenerate the expected files.
