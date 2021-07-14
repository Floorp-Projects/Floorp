# Glean Interface For Firefox Telemetry (GIFFT)

To make Migration from Firefox Telemetry to Glean easier,
the C++ and JS Glean API can be configured
(on a metric-by-metric basis)
to mirror data collection to both the Glean metric and a Telemetry probe.

GIFFT should ideally be used only when the data you require for analysis still mostly lives in Telemetry,
and should be removed promptly when no longer needed.
Instrumentors are encouraged to have the Telemetry probe expire no later than Firefox 98
([scheduled for 2021-12-02](https://wiki.mozilla.org/Release_Management/Calendar)).

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
| [boolean](https://mozilla.github.io/glean/book/user/metrics/boolean.html) | [Scalar of kind: boolean](../telemetry/collection/scalars.html) |
| [labeled_boolean](https://mozilla.github.io/glean/book/user/metrics/labeled_booleans.html) | [Keyed scalar of kind: boolean](../telemetry/collection/scalars.html) |
| [counter](https://mozilla.github.io/glean/book/user/metrics/counter.html) | [Scalar of kind: uint](../telemetry/collection/scalars.html) |
| [labeled_counter](https://mozilla.github.io/glean/book/user/metrics/labeled_counters.html) | [Keyed Scalar of kind: uint](../telemetry/collection/scalars.html) |
| [string](https://mozilla.github.io/glean/book/user/metrics/string.html) | [Scalar of kind: string](../telemetry/collection/scalars.html) |
| [labeled_string](https://mozilla.github.io/glean/book/user/metrics/labeled_strings.html) | *No Supported Telemetry Type* |
| [string_list](https://mozilla.github.io/glean/book/user/metrics/string_list.html) | [Keyed Scalar of kind: boolean](../telemetry/collection/scalars.html). The keys are the strings. The values are all `true`. Calling `Set` on the labeled_string is not mirrored (since there's no way to remove keys from a keyed scalar of kind boolean). Doing so will log a warning. |
| [timespan](https://mozilla.github.io/glean/book/user/metrics/timespan.html) | [Scalar of kind: uint](../telemetry/collection/scalars.html). The value is in units of milliseconds. |
| [timing_distribution](https://mozilla.github.io/glean/book/user/metrics/timing_distribution.html) | [Histogram of kind "linear" or "exponential"](../telemetry/collection/histograms.html#exponential). Samples will be in units of milliseconts. |
| [memory_distribution](https://mozilla.github.io/glean/book/user/metrics/memory_distribution.html) | [Histogram of kind "linear" or "exponential"](../telemetry/collection/histograms.html#exponential). Samples will be in `memory_unit` units. |
| [custom_distribution](https://mozilla.github.io/glean/book/user/metrics/custom_distribution.html) | [Histogram of kind "linear" or "exponential"](../telemetry/collection/histograms.html#exponential). Samples will be used as is. Ensure the bucket count and range match. |
| [uuid](https://mozilla.github.io/glean/book/user/metrics/uuid.html) | [Scalar of kind: string](../telemetry/collection/scalars.html). Value will be in canonical 8-4-4-4-12 format. Value is not guaranteed to be valid, and invalid values may be present in the mirrored scalar which the uuid metric remains empty. Calling `GenerateAndSet` on the uuid is not mirrored, and will log a warning. |
| [datetime](https://mozilla.github.io/glean/book/user/metrics/datetime.html) | [Scalar of kind: string](../telemetry/collection/scalars.html). Value will be in ISO8601 format. |
| [events](https://mozilla.github.io/glean/book/user/metrics/event.html) | [Events](../telemetry/collection/events.html). The `value` field will be left empty.  |
| [quantity](https://mozilla.github.io/glean/book/user/metrics/quantity.html) | [Scalar of kind: uint](../telemetry/collection/scalars.html) |

### The `telemetry_mirror` property in `metrics.yaml`

You must use the C++ enum value of the Histogram, Scalar, or Event being mirrored to.

For example, for this Scalar of kind `boolean`:
```yaml
category.name:
  probe_name:
    kind: boolean
    ...
```

You must provide the following `telemetry_mirror` name for its source
`boolean` metric's definition:

```yaml
    telemetry_mirror: CATEGORY_NAME_PROBE_NAME
```

If you get this wrong it will manifest as a build error.

## Artifact Build Support

Sadly, GIFFT will have no support for Artifact builds even when
[support is added to FOG](https://bugzilla.mozilla.org/show_bug.cgi?id=1698184).
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
[Glean metrics do not](ipc.md).
This may manifest as curious anomalies when comparing the Glean metric to its Telemetry mirror probe.
Ensure your analyses are aggregating Telemetry values from all processes,
or define and use process-specific Glean metrics and Telemetry mirror probes to keep things separate.

### Pings

Glean and Telemetry both send their built-in pings on their own schedules.
This means the values present in these pings may not agree since they reflect state at different time.

For example, if you are measuring "Number of Monitors" with a
[`quantity`](https://mozilla.github.io/glean/book/user/metrics/quantity.html)
sent by default in the Glean "metrics" ping mirrored to a
[Scalar of kind: uint](../telemetry/collection/scalars.html)
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
[label format](https://mozilla.github.io/glean/book/user/metrics/index.html#label-format).

Keyed Scalars, on the other hand, do not have a concept of an "Invalid key".
Firefox Telemetry will accept just about any sequence of bytes as a key.

This means that a label deemed invalid by the Glean SDK may appear in the mirrored probe's data.
For example, using `InvalidLabel` as a label that doesn't conform to the format
(it has upper-case letters)
see that the `labeled_boolean` metric
[correctly ascribes it to `__other__`](https://mozilla.github.io/glean/book/user/metrics/index.html#labeled-metrics)
whereas the mirrored Keyed Scalar with kind boolean stores and retrieves it without change:
```js
Glean.testOnly.mirrorsForLabeledBools.InvalidLabel.set(true);
Assert.equal(true, Glean.testOnly.mirrorsForLabeledBools.__other__.testGetValue());
let snapshot = Services.telemetry.getSnapshotForKeyedScalars().parent;
Assert.equal(true, snapshot["telemetry.test.mirror_for_labeled_bool"]["InvalidLabel"]);
```

### Telemetry Events

A Glean event can be mirrored to a Telemetry Event.
Telemetry Events must be enabled before they can be recorded to via the API
`Telemetry.setEventRecordingEnabled(category, enable);`.
If the Telemetry Event isn't enabled,
recording to the Glean event will still work,
and the event will be Summarized in Telemetry as all disabled events are.

See
[the Telemetry Event docs](../telemetry/collection/events.html)
for details on how disabled Telemetry Events behave.
