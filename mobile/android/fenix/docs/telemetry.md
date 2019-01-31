# Telemetry

Nightly versions of Fenix (build on Taskcluster) send a "baseline" ping to Mozilla's telemetry service.

## Baseline ping

Fenix crates and tries to send a "baseline" ping when the app goes to the background. This baseline ping is defined by the [Glean](https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean) component and [documented in the Android Components repository](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/docs/baseline.md).