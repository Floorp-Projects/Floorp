# Preferences

## User Preferences

`datareporting.healthreport.uploadEnabled`

This determines whether the Glean SDK is enabled.
It can be controlled by users via `about:preferences#privacy`.
If this is set to false from true, we send a
["deletion-request" ping](https://mozilla.github.io/glean/book/user/pings/deletion_request.html)
and no data collections will be persisted or reported from that point.

## Test-only Preferences

`telemetry.fog.test.localhost_port`

If set to a value `port` which is greater than 0, pings will be sent to
`http://localhost:port` instead of `https://incoming.telemetry.mozilla.org`.
If set to a value which is less than 0,
FOG will take all pings scheduled for upload and drop them on the floor,
telling the Glean SDK that it was sent successfully.
This is how you emulate "recording enabled but upload disabled"
like developer builds have in Firefox Telemetry.
Defaults to 0.

`telemetry.fog.test.activity_limit`
`telemetry.fog.test.inactivity_limit`

This pair of prefs control the length of time of activity before inactivity
(or vice versa)
needed before FOG informs the SDK's Client Activity API that the client was (in)active.
Present to allow testing without figuring out how to mock Rust's clock.
Their values are integer seconds.
Defaults to 120 (activity), 1200 (inactivity).
