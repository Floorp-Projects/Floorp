# Telemetry

Fenix uses Mozilla's telemetry service (Glean) and LeanPlum to measure feature performance and engagement.

## Baseline ping

Fenix creates and tries to send a "baseline" ping when the app goes to the background. This baseline ping is defined by the [Glean](https://github.com/mozilla/glean/tree/master/docs/user/pings) component and [documented in the Android Components repository](https://github.com/mozilla/glean/blob/master/docs/user/pings/baseline.md).

## Metrics ping

Fenix creates and tries to send a "baseline" ping. It is defined inside the [`metrics.yaml`](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) file. This ping includes things like whether or not Fenix is currently the default browser.

## Events

Fenix sends event pings that allows us to measure feature performance. These are defined inside the [`metrics.yaml`](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) file.

## Activation

Fenix sends an activation ping once, at startup. Further documentation can be found in the [`activation` ping](activation.md) docs.

## Leanplum
See [here](https://github.com/mozilla-mobile/fenix/blob/master/docs/mma.md) for details on Leanplum usage in Firefox Preview.

## Crash reporting
See [here](https://github.com/mozilla-mobile/fenix/blob/master/docs/crash-reporting.md) for details on crash reporting in Firefox Preview.
