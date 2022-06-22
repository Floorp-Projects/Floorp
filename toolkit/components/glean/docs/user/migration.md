# Migrating Firefox Telemetry to Glean

This guide aims to help you migrate individual data collections from
[Firefox Telemetry](/toolkit/components/telemetry/index.rst)
to
[Glean][book-of-glean] via [Firefox on Glean](../index.md).

This is intended to be a reference to help you fill out your
[migration worksheet][migration-worksheet],
or for mentally translating Telemetry concepts to Glean ones.

```{contents}
```

## General Things To Bear In Mind

You should familiarize yourself with
[the guide on adding new metrics to Firefox Desktop](new_definitions_file.md).
Its advice stacks with the advice included in this guide as
(once you've figured out what kind) you will indeed be adding new metrics.

There are some other broad topics specific to migrating Firefox Telemetry stuff to Glean stuff:

### Process-Agnosticism: No more `record_in_processes` field

Glean (and thus FOG) [doesn't know anything about processes][ipc-dev-doc]
except what it has to in order to ensure all the data makes it to the parent process.
Firefox Telemetry cared very much about which process was collecting which specific data,
keeping them separate.

If you collect data in multiple processes and wish to keep data from each process type separate,
you will need to provide this separation yourself.

Please see [this dev doc][ipc-dev-doc] for an example of how to do that.

### Channel-Agnosticism: No more `release_channel_collection: opt-out`

FOG doesn't make a differentiation between pre-release Firefox and release Firefox,
except inasmuch as is necessary to put the correct channel in `client_info.app_channel`.

This means all data is collected in all build configurations.

If you wish or are required to only collect your data in pre-release Firefox,
please avail yourself of the `EARLY_BETA_OR_EARLIER` `#define` or `AppConstant`.

### File-level Product Inclusion/Exclusion: No more `products` field

Glean determines which metrics are recorded in which products via
[a dependency tree][repositories-yaml].
This means FOG doesn't distinguish between products at the per-product level.

If some of your metrics are recorded in different sets of products
(e.g. some of your metrics are collected in both Firefox Desktop _and_ Firefox for Android,
but others are Firefox Desktop-specific)
you must separate them into separate [definitions files](new_definitions_file.md).

### Many Definitions Files

Each component is expected to own and care for its own
[metrics definitions files](new_definitions_file.md).
There is no centralized `Histograms.json` or `Scalars.yaml` or `Events.yaml`.

Instead the component being instrumented will have its own `metrics.yaml`
(and `pings.yaml` for any [Custom Pings][custom-pings])
in which you will define the data.

See [this guide](new_definitions_file.md) for details.

### Testing

Firefox Telemetry had very uneven support for testing instrumentation code.
FOG has much better support. Anywhere you can instrument is someplace you can test.

It's as simple as calling `testGetValue`.

All migrated collections are expected to be tested.
If you can't test them, then you'd better have an exceptionally good reason why not.

For more details, please peruse the
[instrumentation testing docs](instrumentation_tests).

## Which Glean Metric Type Should I Use?

Glean uses higher-level metric types than Firefox Telemetry does.
This complicates migration as something that is "just a number"
in Firefox Telemetry might map to any number of Glean metric types.

Please choose the most specific metric type that solves your problem.
This'll make analysis easier as
1. Others will know more about how to analyse the metric from more specific types.
2. Tooling will be able to present only relevant operations for more specific types.

Example:
> In Firefox Telemetry I record the number of monitors attached to the computer that Firefox Desktop is running on.
> I could record this number as a [`string`][string-metric], a [`counter`][counter-metric],
> or a [`quantity`][quantity-metric].
> The `string` is an obvious trap. It doesn't even have the correct data type (string vs number).
> But is it a `counter` or `quantity`?
> If you pay attention to this guide you'll learn that `counter`s are used to accumulate sums of information,
> whereas `quantity` metrics are used to record specific values.
> The "sum" of monitors over time doesn't make sense, so `counter` is out.
> `quantity` is the correct choice.

## Histograms

[Histograms][telemetry-histograms]
are the oldest Firefox Telemetry data type, and as such they've accumulated
([ha!][histogram-accumulate]) the most ways of being used.

### Scalar Values in Histograms: kind `flag` and `count`

If you have a Histogram that records exactly one value,
please scroll down and look at the migration guide for the relevant Scalar:
* For Histograms of kind `flag` see "Scalars of kind `bool`"
* For Histograms of kind `count` see "Scalars of kind `uint`"

### Continuous Distributions: kind `linear` and `exponential`

If the Histogram you wish to migrate is formed of multiple buckets that together form a single continuous range
(like you have buckets 1-5, 6-10, 11-19, and 20-50 - they form the range 1-50),
then you will want a "distribution" metric type in Glean.
Which kind of "distribution" metric type depends on what the samples are.

#### Timing samples - Use Glean's `timing_distribution`

The most common type of continuous distribution in Firefox Telemetry is a histogram of timing samples like
[`GC_MS`][gc-ms].

In Glean this sort of data is recorded using a
[`timing-distribution`][timing-distribution-metric] metric type.

You will no longer need to worry about the range of values or number or distribution of buckets
(represented by the `low`, `high`, `n_buckets`, or `kind` in your Histogram's definition).
Glean uses a [clever automatic bucketing algorithm][timing-distribution-metric] instead.

So for a Histogram that records timing samples like this:

```
  "GC_MS": {
    "record_in_processes": ["main", "content"],
    "products": ["firefox", "geckoview_streaming"],
    "alert_emails": ["dev-telemetry-gc-alerts@mozilla.org", "jcoppeard@mozilla.com"],
    "expires_in_version": "never",
    "releaseChannelCollection": "opt-out",
    "kind": "exponential",
    "high": 10000,
    "n_buckets": 50,
    "bug_numbers": [1636419],
    "description": "Time spent running JS GC (ms)"
  },
```

You will migrate to a `timing_distibution` metric type like this:

```yaml
js:
  gc:
    type: timing_distribution
    time_unit: millisecond
    description: |
      Time spent running the Javascript Garbage Collector.
      Migrated from Firefox Telemetry's `GC_MS`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1636419
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1636419#c8
    data_sensitivity:
      - technical
    notification_emails:
      - dev-telemetry-gc-alerts@mozilla.org
      - jcoppeard@mozilla.com
    expires: never
```

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

#### Memory Samples - Use Glean's `memory_distribution`

Another common content of `linear` or `exponential`
Histograms in Firefox Telemetry is memory samples.
For example, [`MEMORY_TOTAL`][memory-total]'s samples are in kilobytes.

In Glean this sort of data is recorded using a
[`memory-distribution`][memory-distribution-metric] metric type.

You will no longer need to worry about the range of values or number or distribution of buckets
(represented by the `low`, `high`, `n_buckets`, or `kind` in your Histogram's definition).
Glean uses a [clever automatic bucketing algorithm][memory-distribution-metric] instead.

So for a Histogram that records memory samples like this:

```
  "MEMORY_TOTAL": {
    "record_in_processes": ["main"],
    "products": ["firefox", "thunderbird"],
    "alert_emails": ["memshrink-telemetry-alerts@mozilla.com", "amccreight@mozilla.com"],
    "bug_numbers": [1198209, 1511918],
    "expires_in_version": "never",
    "kind": "exponential",
    "low": 32768,
    "high": 16777216,
    "n_buckets": 100,
    "description": "Total Memory Across All Processes (KB)",
    "releaseChannelCollection": "opt-out"
  },
```

You will migrate to a `memory_distribution` metric type like this:

```yaml
memory:
  total:
    type: memory_distribution
    memory_unit: kilobyte
    description: |
      The total memory allocated across all processes.
      Migrated from Telemetry's `MEMORY_TOTAL`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1198209
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1511918
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1511918#c9
    data_sensitivity:
      - technical
    notification_emails:
      - memshrink-telemetry-alerts@mozilla.com
      - amccreight@mozilla.com
    expires: never
```

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

#### Percentage Samples - Comment on bug 1657467

A very common Histogram in Firefox Desktop is a distribution of percentage samples.
[For example, `GC_SLICE_DURING_IDLE`][gc-idle].

Glean doesn't currently have a good metric type for this.
But we [intend to add one][new-metric-percent].
If you are migrating a collection of this type,
please add a comment to the bug detailing which probe you are migrating,
and when you need it migrated by.
We'll prioritize adding this metric type accordingly.

#### Other - Use Glean's `custom_distribution`

Continuous Distribution Histograms have been around long enough to have gotten weird.
If you're migrating one of those histograms with units like
["square root of pixels times milliseconds"][checkerboard-severity],
we have a "catch all" metric type for you: [Custom Distribution][custom-distribution-metric].

Sadly, you'll have to care about the bucketing algorithm and bucket ranges for this one.
So for a Histogram with artisinal samples like:

```
  "CHECKERBOARD_SEVERITY": {
    "record_in_processes": ["main", "content", "gpu"],
    "products": ["firefox", "fennec", "geckoview_streaming"],
    "alert_emails": ["gfx-telemetry-alerts@mozilla.com", "botond@mozilla.com"],
    "bug_numbers": [1238040, 1539309, 1584109],
    "releaseChannelCollection": "opt-out",
    "expires_in_version": "never",
    "kind": "exponential",
    "high": 1073741824,
    "n_buckets": 50,
    "description": "Opaque measure of the severity of a checkerboard event"
  },
```

You will migrate it to a `custom_distribution` like:

```yaml
gfx.checkerboard:
  severity:
    type: custom_distribution
    range_max: 1073741824
    bucket_count: 50
    histogram_type: exponential
    unit: Opaque unit
    description: >
      An opaque measurement of the severity of a checkerboard event.
      This doesn't have units, it's just useful for comparing two checkerboard
      events to see which one is worse, for some implementation-specific
      definition of "worse". The larger the value, the worse the
      checkerboarding.
      Migrated from Telemetry's `CHECKERBOARD_SEVERITY`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1238040
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1539309
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1584109
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1584109#c1
    notification_emails:
      - gfx-telemetry-alerts@mozilla.com
      - botond@mozilla.com
    data_sensitivity:
      - technical
    expires: never
```

**TODO [Bug 1677447](https://bugzilla.mozilla.org/show_bug.cgi?id=1677447):**
Custom Distributions aren't yet implemented in FOG. We're working on it.
When they're done we'll see if they'll support GIFFT like the other distributions.

#### Keyed Histograms with Continuous Sample Distributions - Ask on #glean:mozilla.org for assistance

Glean doesn't currently have a good metric type for keyed continuous distributions
like video play time keyed by codec.
Please [reach out to us][glean-matrix] to explain your use-case.
We will help you either work within what Glean currently affords or
[design a new metric type for you][new-metric-type].

### Discrete Distributions: kind `categorical`, `enumerated`, or `boolean` - Use Glean's `labeled_counter`

If the samples don't fall in a continuous range and instead fall into a known number of buckets,
Glean provides the [Labeled Counter][labeled-counter-metric] for these cases.

Simply enumerate the discrete categories as `labels` in the `labeled_counter`.

For example, for a Histogram of kind `categorical` like:

```
  "AVIF_DECODE_RESULT": {
    "record_in_processes": ["main", "content"],
    "products": ["firefox", "geckoview_streaming"],
    "alert_emails": ["cchang@mozilla.com", "jbauman@mozilla.com"],
    "expires_in_version": "never",
    "releaseChannelCollection": "opt-out",
    "kind": "categorical",
    "labels": [
      "success",
      "parse_error",
      "no_primary_item",
      "decode_error",
      "size_overflow",
      "out_of_memory",
      "pipe_init_error",
      "write_buffer_error",
      "alpha_y_sz_mismatch",
      "alpha_y_bpc_mismatch"
    ],
    "description": "Decode result of AVIF image",
    "bug_numbers": [1670827]
  },
```

You would migrate to a `labeled_counter` like:

```yaml
avif:
  decode_result:
    type: labeled_counter,
    description: |
      Each AVIF image's decode result.
      Migrated from Telemetry's `AVIF_DECODE_RESULT`.
    labels:
      - success
      - parse_error
      - no_primary_item
      - decode_error
      - size_overflow
      - out_of_memory
      - pipe_init_error
      - write_buffer_error
      - alpha_y_sz_mismatch
      - alpha_y_bpc_mismatch
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1670827
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1670827#c9
    data_sensitivity:
      - technical
    notification_emails:
      - cchang@mozilla.com
      - jbauman@mozilla.com
    expires: never
```

**N.B:** Glean Labels have a strict regex.
You may have to transform some categories to
`snake_case` so that they're safe for the data pipeline.

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.
**N.B.:** This will mirror back as a Keyed Scalar of kind `uint`,
not as any kind of Histogram,
so your original un-migrated histogram cannot be used as the mirror.

#### Keyed Histograms with Discrete Sample Distributions: `"keyed": true` and kind `categorical`, `enumerated`, or `boolean` - Comment on bug 1657470

Glean doesn't currently have a good metric type for this.
But we [intend to add one][new-metric-keyed-categorical].
If you are migrating a collection of this type,
please add a comment to the bug detailing which probe you are migrating,
and when you need it migrated by.
We'll prioritize adding this metric type accordingly.

## Scalars

[Scalars][telemetry-scalars] are low-level individual data collections with a variety of uses.

### Scalars of `kind: uint` that you call `scalarAdd` on - Use Glean's `counter`

The most common kind of Scalar is of `kind: uint`.
The most common use of such a scalar is to repeatedly call `scalarAdd`
on it as countable things happen.

The Glean metric type for countable things is [the `counter` metric type][counter-metric].

So for a Scalar like this:

```yaml
script.preloader:
  mainthread_recompile:
    bug_numbers:
      - 1364235
    description:
      How many times we ended up recompiling a script from the script preloader
      on the main thread.
    expires: "100"
    keyed: false
    kind: uint
    notification_emails:
      - dothayer@mozilla.com
      - plawless@mozilla.com
    release_channel_collection: opt-out
    products:
      - 'firefox'
      - 'fennec'
    record_in_processes:
      - 'main'
      - 'content'
```

You will migrate to a `counter` metric type like this:

```yaml
script.preloader:
  mainthread_recompile:
    type: counter
    description: |
      How many times we ended up recompiling a script from the script preloader
      on the main thread.
      Migrated from Telemetry's `script.preloader.mainthread_recompile`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1364235
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1364235#c25
    data_sensitivity:
      - technical
    notification_emails:
      - dothayer@mozilla.com
      - plawless@mozilla.com
    expires: "100"
```

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

#### Keyed Scalars of `kind: uint` that you call `scalarAdd` on - Use Glean's `labeled_counter`

Another very common use of Scalars is to have a Keyed Scalar of
`kind: uint`. This was often used to track UI usage.

This is supported by the [Glean `labeled_counter` metric type][labeled-counter-metric].

So for a Keyed Scalar of `kind: uint` like this:

```yaml
urlbar:
  tips:
    bug_numbers:
      - 1608461
    description: >
      A keyed uint recording how many times particular tips are shown in the
      Urlbar and how often their confirm and help buttons are pressed.
    expires: never
    kind: uint
    keyed: true
    notification_emails:
      - email@example.com
    release_channel_collection: opt-out
    products:
      - 'firefox'
    record_in_processes:
      - main
```

You would migrate it to a `labeled_counter` like this:

```yaml
urlbar:
  tips:
    type: labeled_counter
    description: >
      A keyed uint recording how many times particular tips are shown in the
      Urlbar and how often their confirm and help buttons are pressed.
      Migrated from Telemetry's `urlbar.tips`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1608461
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1608461#c42
    data_sensitivity:
      - interaction
    expires: never
    notification_emails:
      - email@example.com
```

Now, if your Keyed Scalar has a list of known keys,
you should provide it to the `labeled_counter` using the `labels` property like so:

```yaml
urlbar:
  tips:
    type: labeled_counter
    labels:
      - tabtosearch_onboard_shown
      - tabtosearch_shown
      - searchtip_onboard_shown
    ...
```

**N.B:** Glean Labels have a strict regex.
You may have to transform some categories to
`snake_case` so that they're safe for the data pipeline.

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

### Scalars of `kind: uint` that you call `scalarSet` on - Use Glean's `quantity`

Distinct from counts which are partial sums,
Scalars of `kind: uint` that you _set_ could contain just about anything.
The best metric type depends on the type of data you're setting
(See "Other Scalar-ish types" for some possibilities).

If it's a numerical value you are setting, chances are you will be best served by
[Glean's `quantity` metric type][quantity-metric].

For a such a quantitative Scalar like:

```yaml
gfx.display:
  primary_height:
    bug_numbers:
      - 1594145
    description: >
      Height of the primary display, takes device rotation into account.
    expires: never
    kind: uint
    notification_emails:
      - gfx-telemetry-alerts@mozilla.com
      - ktaeleman@mozilla.com
    products:
      - 'geckoview_streaming'
    record_in_processes:
      - 'main'
    release_channel_collection: opt-out
```

You would migrate it to a `quantity` like:

```yaml
gfx.display:
  primary_height:
    type: quantity
    unit: pixels
    description: >
      Height of the primary display, takes device rotation into account.
      Migrated from Telemetry's `gfx.display.primary_height`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1594145
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1687219
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1594145#c4
    data_sensitivity:
      - technical
    notification_emails:
      - gfx-telemetry-alerts@mozilla.com
    expires: never
```

Note the required `unit` property.

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

**IPC Note:** Due to `set` not being a [commutative operation][ipc-docs], using `quantity`
on non-parent processes is forbidden.
This is a restriction that favours correctness over friendliness,
which we may revisit if enough use cases require it.
Please [contact us][glean-matrix] if you'd like us to do so.

#### Keyed Scalars of `kind: uint` that you call `scalarSet` on - Ask on #glean:mozilla.org for assistance

Glean doesn't currently have a good metric type for keyed quantities.
Please [reach out to us][glean-matrix] to explain your use-case.
We will help you either work within what Glean currently affords or
[design a new metric type for you][new-metric-type].

### Scalars of `kind: uint` that you call `scalarSetMaximum` or some combination of operations on - Ask on #glean:mozilla.org for assistance

Glean doesn't currently have a good metric type for dealing with maximums,
or for dealing with values you both count and set.
Please [reach out to us][glean-matrix] to explain your use-case.
We will help you either work within what Glean currently affords or
[design a new metric type for you][new-metric-type].

### Scalars of `kind: string` - Use Glean's `string`

If your string value is a unique identifier, then consider
[Glean's `uuid` metric type][uuid-metric] first.

If the string scalar value doesn't fit that or any other more specific metric type,
then [Glean's `string` metric type][string-metric] will do.

For a Scalar of `kind: string` like:

```yaml
widget:
  gtk_version:
    bug_numbers:
      - 1670145
    description: >
      The version of Gtk 3 in use.
    kind: string
    expires: never
    notification_emails:
      - layout-telemetry-alerts@mozilla.com
    release_channel_collection: opt-out
    products:
      - 'firefox'
    record_in_processes:
      - 'main'
```

You will migrate it to a `string` metric like:

```yaml
widget:
  gtk_version:
    type: string
    description: >
      The version of Gtk 3 in use.
      Migrated from Telemetry's `widget.gtk_version`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1670145
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1670145#c7
    data_sensitivity:
      - technical
    notification_emails:
      - layout-telemetry-alerts@mozilla.com
    expires: never
```

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

**IPC Note:** Due to `set` not being a [commutative operation][ipc-docs], using `string`
on non-parent processes is forbidden.
This is a restriction that favours correctness over friendliness,
which we may revisit if enough use cases require it.
Please [contact us][glean-matrix] if you'd like us to do so.

### Scalars of `kind: boolean` - Use Glean's `boolean`

If you need to store a simple true/false,
[Glean's `boolean` metric type][boolean-metric] is likely best.

If you have more that just `true` and `false` to store,
you may prefer a `labeled_counter`.

For a Scalar of `kind: boolean` like:

```yaml
widget:
  dark_mode:
    bug_numbers:
      - 1601846
    description: >
      Whether the OS theme is dark.
    expires: never
    kind: boolean
    notification_emails:
      - layout-telemetry-alerts@mozilla.com
      - cmccormack@mozilla.com
    release_channel_collection: opt-out
    products:
      - 'firefox'
      - 'fennec'
    record_in_processes:
      - 'main'
```

You would migrate to a `boolean` metric type like:

```yaml
widget:
  dark_mode:
    type: boolean
    description: >
      Whether the OS theme is dark.
      Migrated from Telemetry's `widget.dark_mode`.
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1601846
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1601846#c5
    data_sensitivity:
      - technical
    notification_emails:
      - layout-telemetry-alerts@mozilla.com
      - cmccormack@mozilla.com
    expires: never
```

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

**IPC Note:** Due to `set` not being a [commutative operation][ipc-docs], using `boolean`
on non-parent processes is forbidden.
This is a restriction that favours correctness over friendliness,
which we may revisit if enough use cases require it.
Please [contact us][glean-matrix] if you'd like us to do so.

#### Keyed Scalars of `kind: boolean` - Use Glean's `labeled_boolean`

If you have multiple related true/false values, you may have put them in a
Keyed Scalar of `kind: boolean`.

The best match for this is
[Glean's `labeled_boolean` metric type][labeled-boolean-metric].

For a Keyed Scalar of `kind: boolean` like:

```yaml
devtools.tool:
  registered:
    bug_numbers:
      - 1447302
      - 1503568
      - 1587985
    description: >
      Recorded on enable tool checkbox check/uncheck in Developer Tools options
      panel. Boolean stating if the tool was enabled or disabled by the user.
      Keyed by tool id. Current default tools with their id's are defined in
      https://searchfox.org/mozilla-central/source/devtools/client/definitions.js
    expires: never
    kind: boolean
    keyed: true
    notification_emails:
      - dev-developer-tools@lists.mozilla.org
      - accessibility@mozilla.com
    release_channel_collection: opt-out
    products:
      - 'firefox'
      - 'fennec'
    record_in_processes:
      - 'main'
```

You would migrate to a `labeled_boolean` like:

```yaml
devtools.tool:
  registered:
    type: labeled_boolean
    description: >
      Recorded on enable tool checkbox check/uncheck in Developer Tools options
      panel. Boolean stating if the tool was enabled or disabled by the user.
      Migrated from Telemetry's `devtools.tool`.
    labels:
      - options
      - inspector
      - webconsole
      - jsdebugger
      - styleeditor
      - performance
      - memory
      - netmonitor
      - storage
      - dom
      - accessibility
      - application
      - dark
      - light
    bugs:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1447302
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1503568
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1587985
    data_reviews:
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1447302#c17
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1503568#c3
      - https://bugzilla.mozilla.org/show_bug.cgi?id=1587985#c5
    data_sensitivity:
      - interaction
    notification_emails:
      - dev-developer-tools@lists.mozilla.org
      - accessibility@mozilla.com
    expires: never
```

**N.B:** Glean Labels have a strict regex.
You may have to transform some categories to
`snake_case` so that they're safe for the data pipeline.

**GIFFT:** This type of collection is mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

**IPC Note:** Due to `set` not being a [commutative operation][ipc-docs], using `labeled_boolean`
on non-parent processes is forbidden.
This is a restriction that favours correctness over friendliness,
which we may revisit if enough use cases require it.
Please [contact us][glean-matrix] if you'd like us to do so.

### Other Scalar-ish types: `rate`, `timespan`, `datetime`, `uuid`

The Glean SDK provides some very handy higher-level metric types for specific data.
If your data
* Is two or more numbers that are related (like failure count vs total count),
  then consider the [Glean `rate` metric type][rate-metric].
* Is a single duration or span of time (like how long Firefox takes to start),
  then consider the [Glean `timespan` metric type][timespan-metric].
* Is a single point in time (like the most recent sync time),
  then consider the [Glean `datetime` metric type][datetime-metric].
* Is a unique identifier (like a session id),
  then consider the [Glean `uuid` metric type][uuid-metric].

**GIFFT:** These types of collection are mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

## Events - Use Glean's `event`

[Telemetry Events][telemetry-events]
are a lesser-used form of data collection in Firefox Desktop.
Glean aimed to remove some of the stumbling blocks facing instrumentors when using events
in the [Glean `event` metric type][event-metric]:

* Don't worry about enabling event categories.
  In Glean all `events` are always on.
* No more event `name`.
  Events in Glean follow the same `category.name.metric_name`
  naming structure that other metrics do.
* No more `method`/`object`/`value`.
  Events in Glean are just their identifier and an `extras` key/value dictionary.

Since the two Event types aren't that analogous you will need to decide if your event
* Prefers to put its `method`/`object`/`value` in the `extras` dictionary
* Prefers to fold its `method`/`object`/`value` into its identifier

**GIFFT:** Events are mirrorable back to Firefox Telemetry via the
[Glean Interface For Firefox Telemetry][gifft].
See [the guide][gifft] for instructions.

## Other: Environment, Crash Annotations, Use Counters, Etc - Ask on #glean:mozilla.org for assistance

Telemetry has a lot of collection subsystems build adjacent to those already mentioned.
We have solutions for the common ones,
but they are entirely dependent on the specific use case.
Please [reach out to us][glean-matrix] to explain it to us so we can help you either
work within what Glean currently affords or
[design a new metric type for you][new-metric-type].

[book-of-glean]: https://mozilla.github.io/glean/book/index.html
[gc-ms]: https://glam.telemetry.mozilla.org/firefox/probe/gc_ms/explore
[histogram-accumulate]: https://searchfox.org/mozilla-central/rev/d59bdea4956040e16113b05296c56867f761735b/toolkit/components/telemetry/core/Telemetry.h#61
[ipc-docs]: ../dev/ipc.md
[gifft]: gifft.md
[memory-total]: https://glam.telemetry.mozilla.org/firefox/probe/memory_total/explore
[migration-worksheet]: https://docs.google.com/spreadsheets/d/1uEK7zSIJDcGGmof9NywP5AwaovVQCv_Bm3iNqibtESI/edit#gid=0
[boolean-metric]: https://mozilla.github.io/glean/book/reference/metrics/boolean.html
[labeled-boolean-metric]: https://mozilla.github.io/glean/book/reference/metrics/labeled_booleans.html
[counter-metric]: https://mozilla.github.io/glean/book/reference/metrics/counter.html
[labeled-counter-metric]: https://mozilla.github.io/glean/book/reference/metrics/labeled_counters.html
[string-metric]: https://mozilla.github.io/glean/book/reference/metrics/string.html
[labeled-string-metric]: https://mozilla.github.io/glean/book/reference/metrics/labeled_strings.html
[timespan-metric]: https://mozilla.github.io/glean/book/reference/metrics/timespan.html
[timing-distribution-metric]: https://mozilla.github.io/glean/book/reference/metrics/timing_distribution.html
[memory-distribution-metric]: https://mozilla.github.io/glean/book/reference/metrics/memory_distribution.html
[uuid-metric]: https://mozilla.github.io/glean/book/reference/metrics/uuid.html
[datetime-metric]: https://mozilla.github.io/glean/book/reference/metrics/datetime.html
[event-metric]: https://mozilla.github.io/glean/book/reference/metrics/event.html
[custom-distribution-metric]: https://mozilla.github.io/glean/book/reference/metrics/custom_distribution.html
[quantity-metric]: https://mozilla.github.io/glean/book/reference/metrics/quantity.html
[rate-metric]: https://mozilla.github.io/glean/book/reference/metrics/rate.html
[ipc-dev-doc]: ../dev/ipc.md
[gc-idle]: https://glam.telemetry.mozilla.org/firefox/probe/gc_slice_during_idle/explore
[new-metric-keyed-categorical]: https://bugzilla.mozilla.org/show_bug.cgi?id=1657470
[new-metric-percent]: https://bugzilla.mozilla.org/show_bug.cgi?id=1657467
[new-metric-type]: https://wiki.mozilla.org/Glean/Adding_or_changing_Glean_metric_types
[glean-matrix]: https://chat.mozilla.org/#/room/#glean:mozilla.org
[checkerboard-severity]: https://searchfox.org/mozilla-central/rev/d59bdea4956040e16113b05296c56867f761735b/gfx/layers/apz/src/CheckerboardEvent.cpp#44
[telemetry-events]: /toolkit/components/telemetry/collection/events.rst
[telemetry-scalars]: /toolkit/components/telemetry/collection/scalars.rst
[telemetry-histograms]: /toolkit/components/telemetry/collection/histograms.rst
[repositories-yaml]: https://github.com/mozilla/probe-scraper/blob/main/repositories.yaml
