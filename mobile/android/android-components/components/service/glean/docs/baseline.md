# The `baseline` ping
This ping is intended to provide metrics that are managed by the library itself, and not explicitly
set by the application or included in the application's `metrics.yaml` file.

The `baseline` ping is automatically sent when the application is moved to the *background* and it includes
the following fields:

| Field name | Type | Description |
|---|---|---|
| `duration` | Timespan | The duration, in seconds, of the last foreground session |
| `os` | String | The name of the operating system (e.g. "linux", "android", "ios") |
| `os_version` | String | The version of the operating system |
| `device_manufacturer` | String | The manufacturer of the device |
| `device_model` | String | The model name of the device |
| `architecture` | String | The architecture of the device (e.g. "arm", "x86") |
| `locale` | String | The locale of the application |
