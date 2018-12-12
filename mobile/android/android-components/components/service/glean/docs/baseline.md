# The `baseline` ping
This ping is intended to provide metrics that are managed by the library itself, and not explicitly
set by the application or included in the application's `metrics.yaml` file.

The `baseline` ping is automatically sent when the application is moved to the *background* and it includes
the following fields:

| Field name | Type | Description |
|---|---|---|
| `sessions` | Counter | The number of foreground sessions since the last upload |
| `durations` | Timespan | The combined duration of all the foreground sessions since the last upload |
| `os` | String | The name of the operating system (e.g. "linux", "android", "ios") |
| `os_version` | String | The version of the operating system |
| `device` | String | Manufacturer and model |
| `architecture` | String | The architecture of the device (e.g. "arm", "x86") |
| `timezone` | Number | The timezone of the device, as an offset from UTC |
| `accessibility_services` | StringList | List of accessibility services enabled on the device |
| `locale` | String | The locale of the application |
