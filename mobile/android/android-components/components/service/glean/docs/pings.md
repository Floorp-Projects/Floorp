# glean pings

If data collection is enabled, glean provides a set of pings that are assembled out of the box
without any developer intervention.
Every glean ping is in JSON format and contains a [common section](#the-common-information-section)
with shared information data included under the `ping_info` key.
The following is a list of the pings along with the conditions that triggers them.

- [`baseline` ping](baseline.md)
- `events` ping (TBD)
- `metrics` ping (TBD)

## The `ping_info` section
The following fields are always included in the `ping_info` section, for every ping.

| Field name | Type | Description |
|---|---|---|
| `ping_type` | String | The name of the ping type (e.g. "baseline", "metrics") |
| `app_build` | String | TBD |
| `telemetry_sdk_build` | String | The version of the glean library |
| `client_id` | UUID |  A UUID identifying a profile and allowing user-oriented correlation of data |
| `seq` | Counter | A running counter of the number of times pings of this type have been sent |
| `experiments` | Object | A dictionary of [active experiments](#the-experiments-object) |
| `start_time` | Datetime | The time of the start of collection of the data in the ping |
| `end_time` | Datetime | The time of the end of collection of the data in the ping. This is also the time this ping was generated and is likely well before ping transmission time |
| `profile_age` | Datetime | TBD |

All the metrics surviving application restarts (e.g. `client_id`, `seq`, ...) are removed once the
application using glean is uninstalled.

### The `experiments` object
This object contains experiments keyed by the experiment `id`. Each listed experiment contains the
`branch` the client is enrolled in and may contain a string to string map with additional data in the
`extra` key. Both the `id` and `branch` are truncated to 30 characters.

```json
{
  "<id>": {
    "branch": "branch-id",
    "extra": {
      "some-key": "a-value"
    }
  }
}
```

