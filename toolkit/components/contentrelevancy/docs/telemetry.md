# Telemetry

This document serves as a complementary doc for all the telemetry we collect for `contentrelevancy`.
Note that we use FoG ([Firefox on Glean][FoG]) to record telemetry,
all the metrics described below follow the standard Glean metric types.
You can reference `metrics.yaml` or [Glean Dictionary][glean-dictionary] for more details for each metric.

## Classification Metrics - `relevancyClassify`

When the `contentrelevancy` feature is enabled, it will periodically perform classifications in the background.
The following metrics will be recorded for each classification.

### Events

#### `succeed`

This is recorded when a classification is successfully performed.
Various counters are recorded in the `extra_keys` to measure the classification results.

Example:

```
"extra_keys": {
    "input_size": 100,             // The input size was 100 items
    "input_classified_size": 50,   // 50 items (out of 100) were classified with at least one conclusive interest
    "input_inconclusive_size": 10, // 10 items were classified with the inconclusive interest
    "output_interest_size": 4      // The total number of unique interests classified
    "interest_top_1_hits": 20,     // 20 items were classified with the interest with the most hits
    "interest_top_2_hits": 15,     // 15 items were classified with the interest with the 2nd most hits
    "interest_top_3_hits": 10,     // 10 items were classified with the interest with the 3rd most hits
}
```

#### `fail`

This is recorded when a classification is failed or aborted.
The `reason` of the failure is recorded in the `extra_keys`.

```
"extra_keys": {
    "reason": "insufficient-input"  // The classification was aborted due to insufficient input.
                                    // `store-not-ready` indicates the store is null.
                                    // `component-errors` indicates an error in the Rust component.
}
```

### Timing Distribution

#### `duration`

This records the time duration (in milliseconds) of a successful classification.
The durations of unsuccessful classifications are not measured.

## Changelog

### 2024-04

* [Bug 1889404][bug-1889404]: Added basic metrics for relevancy manager

[FoG]: https://firefox-source-docs.mozilla.org/toolkit/components/glean/index.html
[glean-dictionary]: https://dictionary.telemetry.mozilla.org/
[bug-1889404]: https://bugzilla.mozilla.org/show_bug.cgi?id=1889404
