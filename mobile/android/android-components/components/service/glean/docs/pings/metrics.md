# The `metrics` ping

## Description
The `metrics` ping is intended for all of the metrics that are explicitly set by the application or
are included in the application's `metrics.yaml` file (except events). The reported data is tied to the ping's
*measurement window*, which is the time between the collection of two `metrics` ping. Ideally, this
window is expected to be about 24 hours, given that the collection is scheduled daily at 4AM. Data
in the [`ping_info`](pings.md#the-ping_info-section) section of the ping can be used to infer the
length of this window.

## Scheduling
The desired behaviour is to collect the ping at the first available opportunity after 4AM local
time on a new calendar day. This breaks down into two scenarios:

1. the application was just started (after a crash or a long inactivity period);
2. the application was open and the 4AM due time was hit.

In the first case, if the `metrics` ping was already collected on the current calendar day, a new
collection will be scheduled for the next calendar day, at 4AM. If no collection happened yet,
and the due time for the current calendar day has passed, a `metrics` ping is immediately generated
and scheduled for sending.

In the second case, similarly to the previous case, if the `metrics` ping was already collected on the
current calendar day when we hit 4AM, then a new collection is scheduled for the next calendar day.
Otherwise, the `metrics` is immediately collected and scheduled for sending.

More [scheduling examples](#Scheduling-examples) are included below.

## Contents
The `metrics` ping contains all of the metrics defined in `metrics.yaml` (except events) that don't
specify a ping or where `default` is specified in their [`send in pings`](https://mozilla.github.io/glean_parser/metrics-yaml.html#send-in-pings) property.

Additionally, error metrics in the `glean.error` category are included in the `metrics` ping.

The `metrics` ping shall also include the common
[`ping_info`](pings.md#the-ping_info-section) and
['client_info'](pings.md#the-client_info-section) sections.

### Querying ping contents
A quick note about querying ping contents (i.e. for https://sql.telemetry.mozilla.org):  Each metric
in the metrics ping is organized by its metric type, and uses a namespace of 'glean.metrics'. For
instance, in order to select a String field called `test` you would use `metrics.string['glean.metrics.test']`.

### Example metrics ping

```json
{
  "ping_info": {
    "ping_type": "metrics",
    "experiments": {
      "third_party_library": {
        "branch": "enabled"
      }
    },
    "seq": 0,
    "start_time": "2019-03-29T09:50-04:00",
    "end_time": "2019-03-29T10:02-04:00"
  },
  "client_info": {
    "telemetry_sdk_build": "0.49.0",
    "first_run_date": "2019-03-29-04:00",
    "os": "Android",
    "android_sdk_version": "27",
    "os_version": "8.1.0",
    "device_manufacturer": "Google",
    "device_model": "Android SDK built for x86",
    "architecture": "x86",
    "app_build": "1",
    "app_display_version": "1.0",
    "client_id": "35dab852-74db-43f4-8aa0-88884211e545"
  },
  "metrics": {
    "counter": {
      "sample_metrics.test": 1
    },
    "string": {
      "basic.os": "Android"
    },
    "timespan": {
      "test.test_timespan": {
        "time_unit": "microsecond",
        "value": 181908
      }
    }
  }
}
```

## Scheduling Examples

### Crossing due time with the application closed
1. The application is opened on Feb 7 on 3PM, closed on 3:05PM.

    * Glean records one metric A (say startup time in ms) during this measurement window MW1.

2. The application is opened again on Feb 8 on 5PM.

  * Glean notes that we passed local 4AM since MW1.
  * Glean closes MW1, with:

      * `start_time=Feb7/3PM`;
      * `end_time=Feb8/5PM`.

  * Glean records metric A again, into MW2, which has a start_time of Feb8/5PM.

### Crossing due time and changing timezones
1. The application is opened on Feb 7 on 3PM in timezone UTC, closed on 3:05PM.

    * Glean records one metric A (say startup time in ms) during this measurement window MW1.

2. The application is opened again on Feb 8 on 5PM in timezone UTC+1.
    * Glean notes that we passed local 4AM UTC+1 since MW1.
    * Glean closes MW1, with:

        * `start_time=Feb7/3PM/UTC`;
        * `end_time=Feb8/5PM/UTC+1`.

    * Glean records metric A again, into MW2.

### The application doesn’t run in a week
1. The application is opened on Feb 7 on 3PM in timezone UTC, closed on 3:05PM.

    * Glean records one metric A (say startup time in ms) during this measurement window MW1.

2. The application is opened again on Feb 16 on 5PM in timezone UTC.

    * Glean notes that we passed local 4AM UTC since MW1.
    * Glean closes MW1, with:

        * `start_time=Feb7/3PM/UTC`;
        * `end_time=Feb16/5PM/UTC`.

    * Glean records metric A again, into MW2.

### The application doesn’t run for a week, and when it’s finally re-opened the timezone has changed
1. The application is opened on Feb 7 on 3PM in timezone UTC, closed on 3:05PM.

    * Glean records one metric A (say startup time in ms) during this measurement window MW1.

2. The application is opened again on Feb 16 on 5PM in timezone UTC+1.

    * Glean notes that we passed local 4AM UTC+1 since MW1.
    * Glean closes MW1, with:

        * `start_time=Feb7/3PM/UTC`
        * `end_time=Feb16/5PM/UTC+1`.

    * Glean records metric A again, into MW2.

### The user changes timezone in an extreme enough fashion that they cross 4AM twice on the same date
1. The application is opened on Feb 7 at 3PM in timezone UTC+11, closed at 3:05PM

    * Glean records one metric A (say startup time in ms) during this measurement window MW1.

2. The application is opened again on Feb 8 at 4:30AM in timezone UTC+11.

    * Glean notes that we passed local 4AM UTC+11.
    * Glean closes MW1, with:

        * `start_time=Feb7/3PM/UTC+11`;
        * `end_time=Feb8/4:30AM/UTC+11`.

    * Glean records metric A again, into MW2.

3. The user changes to timezone UTC-10 and opens the application at Feb 7 at 10PM in timezone UTC-10

    * Glean records metric A again, into MW2 (not MW1, which was already sent).

4. The user opens the application at Feb 8 5AM in timezone UTC-10

    * Glean notes that we have not yet passed local 4AM on Feb 9
    * Measurement window MW2 remains the current measurement window

5. The user opens the application at Feb 9 7AM in timezone UTC-10

    * Glean notes that we have passed local 4AM on Feb 9
    * Glean closes MW2 with:

        * `start_time=Feb8/4:30AM/UTC+11`;
        * `end_time=Feb9/7AM/UTC-10`.

    * Glean records metric A again, into MW3.
