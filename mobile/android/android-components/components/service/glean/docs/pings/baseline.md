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
| `os` | String | The name of the operating system (e.g. "linux", "Android", "ios") |
| `os_version` | String | The version of the operating system |
| `device_manufacturer` | String | The manufacturer of the device |
| `device_model` | String | The model name of the device |
| `architecture` | String | The architecture of the device (e.g. "arm", "x86") |
| `locale` | String | The locale of the application |

The `baseline` ping shall also include the common [`ping_info`](pings.md#the-ping_info-section) section found in all pings.

### Querying ping contents
A quick note about querying ping contents (i.e. for https://sql.telemetry.mozilla.org):  Each metric
in the baseline ping is organized by its metric type, and uses a namespace of 'glean.baseline'. For 
instance, in order to select `duration` you would use `metrics.timespan['glean.baseline.duration']`. 
If you were trying to select a String based metric such as `os`, then you would use `metrics.string['glean.baseline.os']`