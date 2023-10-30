# Inter-process Communication (IPC)

Firefox Desktop is a multi-process desktop application.
Code requiring instrumentation may be on any of its processes,
so FOG provide facilities to do just that.

## Design

The IPC Design of FOG was worked out in
[bug 1618253](https://bugzilla.mozilla.org/show_bug.cgi?id=1618253).

It centred around a few specific concepts:

### Forbidding Non-Commutative Operations

Because we cannot nicely impose a canonical ordering of metric operations across all processes,
FOG forbids non-[commutative](https://en.wikipedia.org/wiki/Commutative_property)
metric operations in some circumstances.

For example,
`Add()`-ing to a Counter metric works from multiple processes because the order doesn't matter.
However, given a String metric being `Set()` from multiple processes simultaneously,
which value should it take?

This ambiguity is not a good foundation to build trust on,
so we forbid setting a String metric from multiple processes.

#### List of Forbidden Operations

* Boolean's `set` (this is the metric type's only operation)
* Labeled Boolean's `set` (this is the metric type's only operation)
* String's `set` (this is the metric type's only operation)
* Labeled String's `set` (this is the metric type's only operation)
* String List's `set`
    * `add` is permitted (order and uniqueness are not guaranteed)
* Timespan's `start`, `stop`, and `cancel` (these are the metric type's only operations)
* UUID's `set` and `generateAndSet` (these are the metric type's only operations)
* Datetime's `set` (this is the metric type's only operation)
* Quantity's `set` (this is the metric type's only operation)

This list may grow over time as new metric types are added.
If there's an operation/metric type on this list that you need to use in a non-parent process,
please reach out
[on the #glean channel](https://chat.mozilla.org/#/room/#glean:mozilla.org)
and we'll help you out.

### Process Agnosticism

For metric types that can be used cross-process,
FOG provides no facility for identifying which process the instrumentation is on.

What this means is that if you accumulate to a
[Timing Distribution](https://mozilla.github.io/glean/book/user/metrics/timing_distribution.html)
in multiple processes,
all the samples from all the processes will be combined in the same metric.

If you wish to distinguish samples from different process types,
you will need multiple metrics and inline code to select the proper one for the given process.
For example:

```C++
if (XRE_GetProcessType() == GeckoProcessType_Default) {
  mozilla::glean::performance::cache_size.Accumulate(numBytes / 1024);
} else {
  mozilla::glean::performance::non_main_process_cache_size.Accumulate(numBytes / 1024);
}
```

### Scheduling

FOG makes no guarantee about when non-main-process metric values are sent across IPC.
FOG will try its best to schedule opportunistically in idle moments,
and during orderly shutdowns.

There are a few cases where we provide more firm guarantees:

#### Tests

There are test-only APIs in Rust, C++, and Javascript.
These do not await a flush of child process metric values.
You can use the test-only method `testFlushAllChildren` on the `FOG`
XPCOM component to await child data's arrival:
```js
await Services.fog.testFlushAllChildren();
```
See [the test documentation](testing.md) for more details on testing FOG.
For writing tests about instrumentation, see
[the instrumentation test documentation](../user/instrumentation_tests).

#### Pings

We do not guarantee that non-main-process data has made it into a specific ping.

[Built-in pings](https://mozilla.github.io/glean/book/user/pings/index.html)
are submitted by the Rust Glean SDK at times FOG doesn't directly control,
so there may be data not present in the parent process when a built-in ping is submitted.
We don't anticipate this causing a problem since child-process data that
"misses" a given ping will be included in the next one.

At this time,
[Custom Pings](https://mozilla.github.io/glean/book/user/pings/custom.html)
must be sent in the parent process and have no mechanism
to schedule their submission for after child-process data arrives in the parent process.
[bug 1732118](https://bugzilla.mozilla.org/show_bug.cgi?id=1732118)
tracks the addition of such a mechanism or guarantee.

#### Shutdown

We will make a best effort during an orderly shutdown to flush all pending data in child processes.
This means a disorderly shutdown (usually a crash)
may result in child process data being lost.

#### Size

We don't measure or keep an up-to-date calculation of the size of the IPC Payload.
We do, however, keep a count of the number of times the IPC Payload has been accessed.
This is used as a (very) conservative estimate of the size of the IPC Payload so we do not exceed the
[IPC message size limit](https://searchfox.org/mozilla-central/search?q=kMaximumMessageSize).

See [bug 1745660](https://bugzilla.mozilla.org/show_bug.cgi?id=1745660).

### Mechanics

The rough design is that the parent process can request an immediate flush of pending data,
and each child process can decide to flush its pending data whenever it wishes.
The former is via `FlushFOGData() returns (ByteBuf)` and the latter via  `FOGData(ByteBuf)`.

Pending Data is a buffer of bytes generated by `bincode` in Rust in the Child,
handed off to C++, passed over IPC,
then given back to `bincode` in Rust on the Parent.

Rust is then responsible for turning the pending data into
[metrics API][glean-metrics] calls on the metrics in the parent process.

#### Supported Process Types

FOG supports messaging between the following types of child process and the parent process:
* content children (via `PContent`
  (for now. See [bug 1641989](https://bugzilla.mozilla.org/show_bug.cgi?id=1641989))
* gmp children (via `PGMP`)
* gpu children (via `PGPU`)
* rdd children (via `PRDD`)
* socket children (via `PSocketProcess`)
* utility children (via `PUtilityProcess`)

See
[the process model docs](/dom/ipc/process_model.rst)
for more information about what that means.

### Adding Support for a new Process Type

Adding support for a new process type is a matter of extending the two messages
mentioned above in "Mechanics" to another process type's protocol (ipdl file).

1. Add two messages to the appropriate sections in `P<ProcessType>.ipdl`
    * (( **Note:** `PGPU` _should_ be the only ipdl where `parent`
      means the non-parent/-main/-UI process,
      but double-check that you get this correct.))
    * Add `async FOGData(ByteBuf&& aBuf);` to the parent/main/UI process side of things
      (most often `parent:`).
    * Add `async FlushFOGData() returns (ByteBuf buf);` to the non-parent/-main/-UI side
      (most often `child:`).
2. Implement the protocol endpoints in `P<ProcessType>{Child|Parent}.{h|cpp}`
    * The message added to the `parent: ` section goes in
      `P<ProcessType>Parent.{h|cpp}` and vice versa.
3. Add to `FOGIPC.cpp`'s `FlushAllChildData` code that
    1. Enumerates all processes of the newly-supported type (there may only be one),
    2. Calls `SendFlushFOGData on each, and
    3. Adds the resulting promise to the array.
4. Add to `FOGIPC.cpp`'s `SendFOGData` the correct `GeckoProcessType_*`
   enum value, and appropriate code for getting the parent process singleton and calling
   `SendFOGData` on it.
5. Add to the fog crate's `register_process_shutdown` function
   handling for at-shutdown flushing of IPC data.
   If this isn't added, we will log (but not panic)
   on the first use of Glean APIs on an unsupported process type.
    * "Handling" might be an empty block with a comment explaining where to find it
      (like how `PROCESS_TYPE_DEFAULT` is handled)
    * Or it might be custom code
      (like `PROCESS_TYPE_CONTENT`'s)
6. Add to the documented [list of supported process types](#supported-process-types)
   the process type you added support for.

[glean-metrics]: https://mozilla.github.io/glean/book/reference/metrics/index.html
