# Telemetry

Fenix uses Mozilla's telemetry service (Glean) and LeanPlum to measure feature performance and engagement.

## Baseline ping

Fenix creates and tries to send a "baseline" ping when the app goes to the background. This baseline ping is defined by the [Glean](https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean) component and [documented in the Android Components repository](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/docs/pings/baseline.md).

## Metrics ping

Fenix creates and tries to send a "baseline" ping. It is defined inside the [`metrics.yaml`](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) file. This ping includes things like wether or not Fenix is currently the default browser.

## Events

Fenix sends event pings that allows us to measure feature performance. These are defined inside the [`metrics.yaml`](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) file.

## Leanplum Events

| Event           |  Leanplum Key | extras                |
|-----------------|---------------|-----------------------|
| OpenedApp       | E_Opened_App  | source*               |

* `source`: The method used to open Fenix (For exmaple: `app_icon`, `custom_tab` or `link`)