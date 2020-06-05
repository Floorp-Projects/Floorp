# Preferences

## Test-only Preferences

`telemetry.fog.temporary_and_just_for_testing.data_path`

This controls the data path Glean is initialized with (`nightly` only).
It is intended for manual testing only. It will be removed once FOG gets a fixed data path.
By default it is not set. Change requires restart.

`telemetry.fog.test.localhost_port`

If set to a value `port` which is greater than 0, pings will be sent to
`http://localhost:port` instead of `https://incoming.telemetry.mozilla.org`.
Defaults to 0.
