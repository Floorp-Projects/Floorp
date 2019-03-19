# Telemetry

Fenix uses Mozilla's telemetry service (Glean) and LeanPlum to measure feature performance and engagement.

## Baseline ping

Fenix creates and tries to send a "baseline" ping when the app goes to the background. This baseline ping is defined by the [Glean](https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean) component and [documented in the Android Components repository](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/docs/pings/baseline.md).

## Metrics ping

Fenix creates and tries to send a "baseline" ping. It is defined inside the [`metrics.yaml`](https://github.com/mozilla-mobile/fenix/blob/master/app/metrics.yaml) file. This ping includes things like wether or not Fenix is currently the default browser.

## Events

Fenix sends event pings that allows us to measure feature performance.

| Event           | Glean Key         | Leanplum Key | extras                |
|-----------------|-------------------|--------------|-----------------------|
| OpenedApp       | app_opened        | E_Opened_App | source*               |
| SearchBarTapped | search_bar_tapped |              | source**              |
| EnteredUrl      | entered_url       |              | autocomplete***       |
| PerformedSearch | performed_search  |              | search_suggestion**** |

* `source`: The method used to open Fenix (For exmaple: `app_icon` or `link`)
** `source`: The view the user was on when they initiated the search (For example: `Home` or `Browser`)
*** `autocomplete`: A boolean that tells us wether the URL was autofilled by an Autocomplete suggestion
**** `search_suggestion`: A boolean that tells us wether or not the search term was suggested by the Awesomebar