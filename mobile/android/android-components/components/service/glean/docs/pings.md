# glean pings

If data collection is enabled, glean provides a set of pings that are assembled out of the box
without any developer intervention.
Every glean ping is in JSON format and contains a [common section](#the-ping_info-section)
with shared information data included under the `ping_info` key.
The following is a list of the pings along with the conditions that triggers them.

- [`baseline` ping](baseline.md)
- [`events` ping](events.md)
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
| `start_time` | Datetime | The time of the start of collection of the data in the ping | See [note](#a-note-about-time-formats) |
| `end_time` | Datetime | The time of the end of collection of the data in the ping. This is also the time this ping was generated and is likely well before ping transmission time | See [note](#a-note-about-time-formats) |
| `profile_age` | Datetime | TBD |

All the metrics surviving application restarts (e.g. `client_id`, `seq`, ...) are removed once the
application using glean is uninstalled.

### A note about time formats
Time formats are used and expected in ISO 8601 format.  Due to the minimum Android SDK version 21
not having direct support of outputting or parsing these formats, there was difficulty in finding
a way to output these formats with the `:` character properly encoded in the timezone offset.  So
we get the following:

```
2018-12-19T12:36-0600
```

We require the following to comply with certain back-end services:

```
2018-12-19T12:36-06:00
```

Which is why the `:` is manually inserted in order to comply with the format and requirements.

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

## Ping submission
The pings glean generates are submitted to the Mozilla servers at specific paths, in order to provide
additional metadata without the need of unpacking the ping payload. A typical submission URL looks like
`"<server-address>/submit/<application-id>/<doc-type>/<glean-schema-version>/<ping-uuid>"`, with:
 
- `<server-address>`: the address of the server that receives the pings;
- `<application-id>`: a unique application id, automatically detected by glean; this is the value returned by [`Context.getPackageName()`](http://developer.android.com/reference/android/content/Context.html#getPackageName());
- `<doc-type>`: the name of the ping; this can be one of the pings available out of the box with glean, or a custom ping;
- `<glean-schema-version>`: the version of the glean ping schema;
- `<ping-uuid>`: a unique identifier for this ping.
