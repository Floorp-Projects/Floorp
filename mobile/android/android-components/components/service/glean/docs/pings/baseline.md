# The `baseline` ping

## Description
This ping is intended to provide metrics that are managed by the library itself, and not explicitly
set by the application or included in the application's `metrics.yaml` file.

## Scheduling
The `baseline` ping is automatically sent when the application is moved to the [background](pings.md#defining-background-state) and it includes
the following fields:

## Contents
| Field name | Type | Description |
|---|---|---|
| `duration` | Timespan | The duration, in seconds, of the last foreground session |
| `locale` | String | The locale of the application |

The `baseline` ping shall also include the common [ping sections](pings.md) found in all pings.

### Querying ping contents
A quick note about querying ping contents (i.e. for https://sql.telemetry.mozilla.org):  Each metric
in the baseline ping is organized by its metric type, and uses a namespace of `glean.baseline`. For
instance, in order to select `duration` you would use `metrics.timespan['glean.baseline.duration']`.
If you were trying to select a String based metric such as `os`, then you would use `metrics.string['glean.baseline.os']`

## Example baseline ping

```json
{
  "ping_info": {
    "ping_type": "baseline",
    "experiments": {
      "third_party_library": {
        "branch": "enabled"
      }
    },
    "seq": 0,
    "start_time": "2019-03-29T09:50-04:00",
    "end_time": "2019-03-29T09:53-04:00"
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
    "string": {
      "glean.baseline.locale": "en-US",
      "glean.baseline.duration": {
        "value": 52,
        "time_unit": "second"
      }
    }
  }
}
```
