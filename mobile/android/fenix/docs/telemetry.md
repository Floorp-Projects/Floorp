# Telemetry

Fenix uses Mozilla's telemetry service (Glean) and LeanPlum to measure feature performance and engagement.

## Baseline ping

Fenix creates and tries to send a "baseline" ping when the app goes to the background. This baseline ping is defined by the [Glean](https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean) component and [documented in the Android Components repository](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/docs/baseline.md).

## Events

| Event     | Glean Key | Leanplum Key | extras |
|-----------|-----------|--------------|--------|
| OpenedApp |           | E_Opened_App |        |