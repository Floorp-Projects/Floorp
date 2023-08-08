# Glean Interface For Firefox Telemetry (GIFFT)

To make Migration from Firefox Telemetry to Glean easier,
the C++ and JS Glean API can be configured
(on a metric-by-metric basis)
to mirror data collection to both the Glean metric and a Telemetry probe.

GIFFT should ideally be used only when the data you require for analysis still mostly lives in Telemetry,
and should be removed promptly when no longer needed.
Instrumentors are encouraged to have the Telemetry mirror probe expire within six versions.
(As always you can renew an expiring probe if you're still using it,
but this will help us get closer to the time when we eventually turn Telemetry off.)

**Note:** GIFFT only works for data provided via C++ or JS.
Rust Glean metrics APIs will not mirror to Telemetry as Telemetry does not have a Rust API.

**Note:** Using the Glean API replaces the Telemetry API.
Do not use any mix of the two APIs for the same probe.

## How to Mirror a Glean Metric to a Firefox Telemetry Probe

For the mirror to work, you need three things:
* A compatible Glean metric (defined in a `metrics.yaml`)
* A compatible Telemetry probe
  (defined in `Histograms.json`, `Scalars.yaml`, or `Events.yaml`)
* A `telemetry_mirror` property on the Glean metric definition identifying the Telemetry probe

### Compatibility

This compatibility table explains which Telemetry probe types can be mirrors for which Glean metric types:

| Glean Metric Type | Telementry Probe Type |
| ----------------- | --------------------- |
| [boolean](https://mozilla.github.io/glean/book/reference/metrics/boolean.html) | [Scalar of kind: boolean](/toolkit/components/telemetry/collection/scalars.html) |
| [labeled_boolean](https://mozilla.github.io/glean/book/reference/metrics/labeled_booleans.html) | [Keyed scalar of kind: boolean](/toolkit/components/telemetry/collection/scalars.html) |
| [counter](https://mozilla.github.io/glean/book/reference/metrics/counter.html) | [Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.html) |
| [labeled_counter](https://mozilla.github.io/glean/book/reference/metrics/labeled_counters.html) | [Keyed Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.html) |
| [string](https://mozilla.github.io/glean/book/reference/metrics/string.html) | [Scalar of kind: string](/toolkit/components/telemetry/collection/scalars.html) |
| [labeled_string](https://mozilla.github.io/glean/book/reference/metrics/labeled_strings.html) | *No Supported Telemetry Type* |
| [string_list](https://mozilla.github.io/glean/book/reference/metrics/string_list.html) | [Keyed Scalar of kind: boolean](/toolkit/components/telemetry/collection/scalars.html). The keys are the strings. The values are all `true`. Calling `Set` on the labeled_string is not mirrored (since there's no way to remove keys from a keyed scalar of kind boolean). Doing so will log a warning. |
| [timespan](https://mozilla.github.io/glean/book/reference/metrics/timespan.html) | [Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.html). The value is in units of milliseconds. |
| [timing_distribution](https://mozilla.github.io/glean/book/reference/metrics/timing_distribution.html) | [Histogram of kind "linear" or "exponential"](/toolkit/components/telemetry/collection/histograms.html#exponential). Samples will be in units of milliseconts. |
| [memory_distribution](https://mozilla.github.io/glean/book/reference/metrics/memory_distribution.html) | [Histogram of kind "linear" or "exponential"](/toolkit/components/telemetry/collection/histograms.html#exponential). Samples will be in `memory_unit` units. |
| [custom_distribution](https://mozilla.github.io/glean/book/reference/metrics/custom_distribution.html) | [Histogram of kind "linear" or "exponential"](/toolkit/components/telemetry/collection/histograms.html#exponential). Samples will be used as is. Ensure the bucket count and range match. |
| [uuid](https://mozilla.github.io/glean/book/reference/metrics/uuid.html) | [Scalar of kind: string](/toolkit/components/telemetry/collection/scalars.html). Value will be in canonical 8-4-4-4-12 format. Value is not guaranteed to be valid, and invalid values may be present in the mirrored scalar while the uuid metric remains empty. Calling `GenerateAndSet` on the uuid is not mirrored, and will log a warning. |
| [url](https://mozilla.github.io/glean/book/reference/metrics/url.html) | [Scalar of kind: string](/toolkit/components/telemetry/collection/scalars.html). The stringified Url will be cropped to the maximum length allowed by the legacy type. |
| [datetime](https://mozilla.github.io/glean/book/reference/metrics/datetime.html) | [Scalar of kind: string](/toolkit/components/telemetry/collection/scalars.html). Value will be in ISO8601 format. |
| [events](https://mozilla.github.io/glean/book/reference/metrics/event.html) | [Events](/toolkit/components/telemetry/collection/events.html). The `value` field will be left empty.  |
| [quantity](https://mozilla.github.io/glean/book/reference/metrics/quantity.html) | [Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.html) |
| [rate](https://mozilla.github.io/glean/book/reference/metrics/rate.html) | [Keyed Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.html). The keys are "numerator" and "denominator". Does not work for `rate` metrics with external denominators. |
| [text](https://mozilla.github.io/glean/book/reference/metrics/text.html) | *No Supported Telemetry Type* |

### The `telemetry_mirror` property in `metrics.yaml`

You must use the C++ enum identifier of the Histogram, Scalar, or Event being mirrored to:
* For Histograms, the Telemetry C++ enum identifier is the histogram's name
    * e.g. The C++ enum identifier for `WR_RENDERER_TIME` is
      `WR_RENDERER_TIME` (see {searchfox}`gfx/metrics.yaml`)
* For Scalars, the Telemetry C++ enum identifier is the Scalar category and name in
  `SCREAMING_SNAKE_CASE` with any `.` replaced with `_`
    * e.g. The enum identifier for `extensions.startupCache.load_time` is
      `EXTENSIONS_STARTUPCACHE_LOAD_TIME` (see {searchfox}`toolkit/components/extensions/metrics.yaml`)
* For Events, the Telemetry C++ enum identifier is the Event category, method, and object
  rendered in `Snakey_CamelCase`.
    * e.g. The enum identifier for `page_load.toplevel#content` is
      `Page_load_Toplevel_Content` (see {searchfox}`dom/metrics.yaml`)

If you use the wrong enum identifier, this will manifest as a build error.

If you are having trouble finding the correct conjugation for the mirror Telemetry probe,
you can find the specific value in the list of all Telemetry C++ enum identifiers in
`<objdir>/toolkit/components/telemetry/Telemetry{Histogram|Scalar|Event}Enums.h`.
(Choose the file appropriate to the type of the Telemetry mirror.)

## Artifact Build Support

Sadly, GIFFT does not support Artifact builds.
You must build Firefox when you add the mirrored metric so the C++ enum value is present,
even if you only use the metric from Javascript.

## Analysis Gotchas

Firefox Telemetry and the Glean SDK are very different.
Though GIFFT bridges the differences as best it can,
there are many things it cannot account for.

These are a few of the ways that differences between Firefox Telemetry and the Glean SDK might manifest as anomalies during analysis.

### Processes, Products, and Channels

Like Firefox on Glean itself,
GIFFT doesn't know what process, product, or channel it is recording in.
Telemetry does, and imposes restrictions on which probes can be recorded to and when.

Ensure that the following fields in any Telemetry mirror's definition aren't too restrictive for your use:
* `record_in_processes`
* `products`
* `release_channel_collection`/`releaseChannelCollection`

A mismatch won't result in an error.
If you, for example,
record to a Glean metric in a release channel that the Telemetry mirror probe doesn't permit,
then the Glean metric will have a value and the Telemetry mirror probe won't.

Also recall that Telemetry probes split their values across processes.
[Glean metrics do not](../dev/ipc.md).
This may manifest as curious anomalies when comparing the Glean metric to its Telemetry mirror probe.
Ensure your analyses are aggregating Telemetry values from all processes,
or define and use process-specific Glean metrics and Telemetry mirror probes to keep things separate.

### Pings

Glean and Telemetry both send their built-in pings on their own schedules.
This means the values present in these pings may not agree since they reflect state at different time.

For example, if you are measuring "Number of Monitors" with a
[`quantity`](https://mozilla.github.io/glean/book/reference/metrics/quantity.html)
sent by default in the Glean "metrics" ping mirrored to a
[Scalar of kind: uint](/toolkit/components/telemetry/collection/scalars.rst)
sent by default in the Telemetry "main" ping,
then if the user plugs in a second monitor between midnight
(when Telemetry "main" pings with reason "daily" are sent) and 4AM
(when Glean "metrics" pings with reason "today" are sent),
the value in the `quantity` will be `2`
while the value in the Scalar of kind: uint will be `1`.

If the metric or mirrored probe are sent in Custom pings,
the schedules could line up exactly or be entirely unrelated.

### Labels

Labeled metrics supported by GIFFT
(`labeled_boolean` and `labeled_counter`)
adhere to the Glean SDK's
[label format](https://mozilla.github.io/glean/book/reference/metrics/index.html#label-format).

Keyed Scalars, on the other hand, do not have a concept of an "Invalid key".
Firefox Telemetry will accept just about any sequence of bytes as a key.

This means that a label deemed invalid by the Glean SDK may appear in the mirrored probe's data.
For example, using 72 "1" characters as a label that doesn't conform to the format
(it is longer than 71 printable ASCII characters).
See that the `labeled_boolean` metric
[correctly ascribes it to `__other__`](https://mozilla.github.io/glean/book/reference/metrics/index.html#labeled-metrics)
whereas the mirrored Keyed Scalar with kind boolean stores and retrieves it without change:
```js
Glean.testOnly.mirrorsForLabeledBools["1".repeat(72)].set(true);
Assert.equal(true, Glean.testOnly.mirrorsForLabeledBools.__other__.testGetValue());
// The above actually throws NS_ERROR_LOSS_OF_SIGNIFICANT_DATA because it also records
// an invalid_label error. But you get the idea.
let snapshot = Services.telemetry.getSnapshotForKeyedScalars().parent;
Assert.equal(true, snapshot["telemetry.test.mirror_for_labeled_bool"]["1".repeat(72)]);
```

### Telemetry Events

A Glean event can be mirrored to a Telemetry Event.
Telemetry Events must be enabled before they can be recorded to via the API
`Telemetry.setEventRecordingEnabled(category, enable);`.
If the Telemetry Event isn't enabled,
recording to the Glean event will still work,
and the event will be Summarized in Telemetry as all disabled events are.

See
[the Telemetry Event docs](/toolkit/components/telemetry/collection/events.rst)
for details on how disabled Telemetry Events behave.

### Numeric Values

The arguments and storage formats for Glean's numeric types
(`counter`, `labeled_counter`, `quantity`, `rate`, and `timespan`)
are different from Telemetry's numeric type
(Scalar of kind `uint`).

This results in a few notable differences.

#### Saturation and Overflow

`counter`, `labeled_counter`, and `rate` metrics are stored as 32-bit signed values.
`quantity` metrics are stored as 64-bit signed values.
`timing_distribution` samples can be 64-bit signed values.
All of these Glean numeric metric types saturate at their maximum representable value,
or according to the Limits section of the Glean metric type documentation.

Scalars of kind `uint` are stored as 32-bit unsigned values.
They will overflow if they exceed the value $2^{32} - 1$.

If a Glean numeric type saturates, it will record an error of type `invalid_overflow`.
In your analyses please check for these errors.

#### Quantity Value Over-size

Values greater than $2^{32} - 1$ passed to a `quantity` metric's
`set()` method will be clamped to $2^{32} - 1$ before being passed to the metric's Telemetry mirror.

#### Negative Values

Values less than 0 passed to any numeric metric type's API will not be passed on to the Telemetry mirror.
This avoids small negative numbers being cast into a stunningly large numbers,
and keeps the Telemetry mirror's value closer to that of the Glean metric.

#### Long Time Spans

If the number of milliseconds between calls to a
`timespan` metric's `start()` and `stop()` methods exceeds $2^{32} - 1$,
the value passed to the metric's Telemetry mirror will be clamped to $2^{32} - 1$.

The same happens for samples in `timing_distribution` metrics:
values passed to the Telemetry mirror histogram will saturate at $2^{32} - 1$
until they get past $2^{64}$ when they'll overflow.

### App Shutdown

Telemetry only works up to
[`ShutdownPhase::AppShutdownTelemetry` aka `profile-before-change-telemetry`][app-shutdown].
Telemetry data recorded after that phase just aren't persisted.

FOG _presently_ shuts down Glean in a later phase,
and so is able to collect data deeper into shutdown.
(The particular phase is not presently something anyone's asked us to guarantee,
so that's why I'm not being precise.)

What this means is that, for data recorded later in shutdown,
Glean will report more complete information than Telemetry will.

[app-shutdown]: https://searchfox.org/mozilla-central/source/xpcom/base/AppShutdown.cpp#57
