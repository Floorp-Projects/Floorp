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

## Defines

`MOZ_GLEAN_ANDROID`

If set, recording Glean metrics are a no-op. Glean will not be initialized.
Only set on Android.
This define will be removed after we sort out how Android and Geckoview will work
(see [bug 1670261](https://bugzilla.mozilla.org/show_bug.cgi?id=1670261)).
It can be queried in C++ via `#ifndef MOZ_GLEAN_ANDROID`,
and in JS via `AppConstants.MOZ_GLEAN_ANDROID`.

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

`MOZILLA_OFFICIAL` tends to be set on most builds released to users,
including builds distributed by Linux distributions.
It tends to not be set on local developer builds.
See [bug 1680025](https://bugzilla.mozilla.org/show_bug.cgi?id=1680025) for details.
