# Preferences and Defines

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
If set to a value `port` which is less than 0, FOG will:
1) Tell Glean that upload is enabled, even if it isn't.
2) Take all pings scheduled for upload and drop them on the floor,
   telling the Glean SDK that it was sent successfully.

This is how you emulate "recording enabled but upload disabled"
like developer builds have in Firefox Telemetry.
When switching from `port < 0` to `port >= 0`,
Glean will be told (if just temporarily) that upload is disabled.
This clears the stores of recorded-but-not-reported data.
Defaults to 0.

`telemetry.fog.test.activity_limit`
`telemetry.fog.test.inactivity_limit`

This pair of prefs control the length of time of activity before inactivity
(or vice versa)
needed before FOG informs the SDK's Client Activity API that the client was (in)active.
Present to allow testing without figuring out how to mock Rust's clock.
Their values are integer seconds.
Defaults to 120 (activity), 1200 (inactivity).

## Defines

`MOZ_GLEAN_ANDROID`

If set, the Glean SDK is assumed to be managed by something other than FOG, meaning:
* [GIFFT][gifft] is disabled.
* FOG doesn't initialize Glean
* FOG doesn't relay (in)activity or experiment annotations to Glean

`MOZILLA_OFFICIAL`

If unset, we set a `glean_disable_upload` Rust feature in
`gkrust` and `gkrust-shared` which is forwarded to `fog_control` as `disable_upload`.
This feature defaults FOG to an "upload disabled"
mode where collection on the client proceeds as normal but no ping is sent.
This mode can be overridden at runtime in two ways:
* If the ping has a
  [Debug Tag](https://mozilla.github.io/glean/book/user/debugging/index.html)
  then it is sent so that it may be inspected in the
  [Glean Debug Ping Viewer](https://debug-ping-preview.firebaseapp.com/).
* If the preference `telemetry.fog.test.localhost_port` is set to a value greater than 0,
  then pings are sent to a server operating locally at that port
  (even if the ping has a Debug Tag), to enable testing.

Also, if set, [JOG](./jog) is disabled.
Artifact builds will not exhibit changes to their Glean metrics.

`MOZILLA_OFFICIAL` tends to be set on most builds released to users,
including builds distributed by Linux distributions.
It tends to not be set on local developer builds.
See [bug 1680025](https://bugzilla.mozilla.org/show_bug.cgi?id=1680025) for details.

`COMPILE_ENVIRONMENT`

If `COMPILE_ENVIRONMENT` isn't set in the build config,
[JOG](./jog) will generate a file for the runtime-registration of metrics and pings.
This is to support [Artifact Builds](/contributing/build/artifact_builds).

`OS_TARGET`

If not set to `'Android'` we set a `glean_million_queue` Rust feature
([see gkrust-features.mozbuild][gkrust-features])
which, when passed to the Glean SDK,
opts us into a preinit queue that doesn't discard tasks until there are 10^6 of them.

See [bug 1797494](https://bugzilla.mozilla.org/show_bug.cgi?id=1797494) for details.

[gkrust-features]: https://searchfox.org/mozilla-central/source/toolkit/library/rust/gkrust-features.mozbuild
[gifft]: ../user/gifft
