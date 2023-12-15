# Migrating Telemetry Collected via Geckoview Streaming to Glean

With Geckoview Streaming (GVST) having been deprecated,
this is a guide to migrating collections to [Glean][book-of-glean]
via [Firefox on Glean](../index.md).

```{contents}
```

## Before we Begin

You should familiarize yourself with the guide on
[Adding New Metrics to Firefox Desktop](./new_definitions_file.md).

You should also read through the guide for
[Migrating Metrics from Legacy Telemetry to Glean](./migration.md).

This guide assumes some basic familiarity with the above.
The [Glean book][book-of-glean] has a full API reference, as well.

## Process

There are 3 main steps:

1. Move the metric definition and make necessary updates
2. Update the call to use the Glean API
3. Update the tests to use the Glean test API

### Move and Update the Metric Definition

Existing metrics that make use of the GVST will already have a fully specified YAML
entry that we will use as a starting point.
This is convenient, but we want to make some minor updates rather than take it fully as is.
At a minimum we need to move the definition out of the
[Geckoview metrics.yaml][gv-metrics-yaml] file and change from GVST to [GIFFT](./gifft.md).
It can go into whichever metrics.yaml file you feel is most appropriate.
If an appropriate one does not exist, create a new one [following this guide][new-yaml].
Completely remove the metric definition from the Geckoview `metrics.yaml`.

For all metric types other than `labeled counters` the first step is to change the key
of the `gecko_datapoint` entry to `telemetry_mirror`.
Next, update the value as per the rules outlined in the [GIFFT guide][telemetry-mirror-doc].
This change is required to keep data flowing in the Legacy Telemetry version of the metric.
Doing so will ensure that downstream analyses do not break unintentionally.
It is not necessary to modify the [Histograms.json][histograms-json] or
[Scalars.yaml][scalars-yaml] file.

To migrate `labeled counters` instead fully remove the `gecko_datapoint` entry.
Note that our overall treatment of this type is slightly different.

Next add the bug that covers the migration to the `bugs` field.
Update the `description` field as well to indicate the metric used to be collected
via the Geckoview Streaming API. Other fields should be updated as makes sense.
For example, you may need to update the field for `notification_emails`.
Since you are not changing the collection a new data review is not necessary.
However, if you notice a metric is set to expire soon and it should continue to be collected,
complete a [data review renewal][dr-renewal] form.

Do not change the name or metric type.
**If you need to change or one both you are creating a new collection.**

### Update Calls to Use the Glean API

The next step is to update the metric calls to the Glean API.
Fortunately, for the vast matjority of metricsthis is a 1:1 swapout,
or for `labeled counters` (which are `categorical histograms` in legacy) we add a second call.
We identify the Glean API, remove the old call, and put in its place the Glean call.
You can find a full API reference in the [Glean][book-of-glean], but we'll look at how to record
values for the types that have existing GVST metrics.

One way to mentally organize the metrics is to break them into two groups, those that are set,
and those that are accumulated to. As when you use the Legacy Telemetry API for GVST,
they are invoked slightly differently.

To record in C++, you need to include `#include "mozilla/glean/GleanMetrics.h"`.
In Javascript, it is extremely unlikely that you will not already have access to `Glean`.
If you do not, please reach out to a Data Collection Tools team member on
[the #glean:mozilla.org Matrix channel](https://chat.mozilla.org/#/room/#glean:mozilla.org).

Let's try a few examples.

#### Migrating a Set Value (string) in C++

Let's look at the case of the Graphics Adaptor Vendor ID.
This is a String,
and it's recorded via GVST in C++

GVST YAML entry (slightly truncated):

```YAML
geckoview:
  version:
    description: >
      The version of the Gecko engine, example: 74.0a1
    type: string
     gecko_datapoint: gecko.version
```

And this is recorded:

```CPP
Telemetry::ScalarSet(Telemetry::ScalarID::GECKO_VERSION,
                         NS_ConvertASCIItoUTF16(gAppData->version));
```

To migrate this, let's update our YAML entry, again moving it out of the GVST
metrics.yaml into the most appropriate one:

```YAML
geckoview:
  version:
    description: >
      The version of the Gecko engine, example: 74.0a1
      (Migrated from geckoview.gfx.adapter.primary)
    type: string
    telemetry_mirror: GECKO_VERSION
```

Notice how we've checked all of our boxes:

* Made sure our category makes sense
* Changed gecko_datapoint to telemetry_mirror
* Updated the description to note that it was migrated from another collection
* Kept the type identical.

Now we can update our call:

```CPP
mozilla::glean::geckoview::version.Set(nsDependentCString(gAppData->version));
```

#### Migrating a Labeled Counter in C++

Let's look at probably the most complicated scenario, one where we need to accumulate
to a labeled collection. Because the glean `labeled counter` and legacy `categorical histogram`
type do not support GIFFT, we will add a second call.
Let's take a look at an elided version of how this would be done with GVST:

```CPP
switch (aResult.as<NonDecoderResult>()) {
  case NonDecoderResult::SizeOverflow:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::size_overflow);
    return;
  case NonDecoderResult::OutOfMemory:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::out_of_memory);
    return;
  case NonDecoderResult::PipeInitError:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::pipe_init_error);
    return;
}
```

And we update it by adding a call with the FOG API:

```CPP
switch (aResult.as<NonDecoderResult>()) {
  case NonDecoderResult::SizeOverflow:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::size_overflow);
    mozilla::glean::avif::decode_result.EnumGet(avif::DecodeResult::eSizeOverflow).Add();
    return;
  case NonDecoderResult::OutOfMemory:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::out_of_memory);
    mozilla::glean::avif::decode_result.EnumGet(avif::DecodeResult::eOutOfMemory).Add();
    return;
  case NonDecoderResult::PipeInitError:
    AccumulateCategorical(LABELS_AVIF_DECODE_RESULT::pipe_init_error);
    mozilla::glean::avif::decode_result.EnumGet(avif::DecodeResult::ePipeInitError).Add();
    return;
}
```

#### Migrating an Accumulated Value (Histogram) in Javascript

Javascript follows the same pattern. Consider the case when want to record the number
of uniqiue site origins. Here's the original JS implementation:

```Javascript
let originCount = this.computeSiteOriginCount(aWindows, aIsGeckoView);
let histogram = Services.telemetry.getHistogramById(
  "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_ALL_TABS",
);

if (!this._lastRecordSiteOrigin) {
  this._lastRecordSiteOrigin = currentTime;
} else if (currentTime >= this._lastRecordSiteOrigin + this.min_interval) {
  this._lastRecordSiteOrigin = currentTime;

  histogram.add(originCount);
}
```

And here is the direct Glean version

```Javascript
let originCount = this.computeSiteOriginCount(aWindows, aIsGeckoView);

if (!this._lastRecordSiteOrigin) {
  this._lastRecordSiteOrigin = currentTime;
} else if (currentTime >= this._lastRecordSiteOrigin + this.min_interval) {
  this._lastRecordSiteOrigin = currentTime;

  Glean.tabs.uniqueSiteOriginsAllTabs.accumulateSamples([originCount]);
}

```

Note that we don't have to call into Services to get the histogram object.

### Update the tests to use the Glean test API

The last piece is updating tests. If tests don't exist
(which is often the case since testing metrics collected via GVST can be challenging),
we recommend that you write them as the
[the Glean test API is quite straightforward](./instrumentation_tests.md).

The main test method is `testGetValue()`. Returning to our earlier example of
Number of Unique Site Origins, in a Javascript test we can invoke:

```Javascript
let result = Glean.tabs.uniqueSiteOriginsAllTabs.testGetValue();

// This collection is a histogram, we can check the sum for this test
Assert.equal(result.sum, 144);
```

If your collection is in a child process, it can be helpful to invoke
`await Services.fog.testFlushAllChildren();`

If you wish to write a C++ test, `testGetValue()` is also our main method:

```CPP
#include "mozilla/glean/GleanMetrics.h"

ASSERT_EQ(1,
          mozilla::glean::avif::image_decode_result
            .EnumGet(avif::DecodeResult::eSizeOverflow)
            .TestGetValue()
            .unwrap()
            .ref());

ASSERT_EQ(3,
          mozilla::glean::avif::image_decode_result
            .EnumGet(avif::DecodeResult::eOutOfMemory)
            .TestGetValue()
            .unwrap()
            .ref());

ASSERT_EQ(0,
          mozilla::glean::avif::image_decode_result
            .EnumGet(avif::DecodeResult::ePipeInitError)
            .TestGetValue()
            .unwrap()
            .ref());
```

[book-of-glean]: https://mozilla.github.io/glean/book/index.html
[gv-metrics-yaml]: https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/geckoview/streaming/metrics.yaml
[histograms-json]: https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Histograms.json
[scalars-yaml]: https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml
[new-yaml]: ./new_definitions_file.md#where-do-i-define-new-metrics-and-pings
[dr-renewal]: https://github.com/mozilla/data-review/blob/main/renewal_request.md
[telemetry-mirror-doc]: https://firefox-source-docs.mozilla.org/toolkit/components/glean/user/gifft.html#the-telemetry-mirror-property-in-metrics-yaml
