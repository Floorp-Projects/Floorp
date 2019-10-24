[android-components](../index.md) / [mozilla.components.service.glean.testing](index.md) / [GleanTestLocalServer](./-glean-test-local-server.md)

# GleanTestLocalServer

`typealias GleanTestLocalServer = GleanTestLocalServer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/testing/GleanTestLocalServer.kt#L27)

This implements a JUnit rule for writing tests for Glean SDK metrics.

The rule takes care of sending Glean SDK pings to a local server, at the
address: "http://localhost:".

This is useful for Android instrumented tests, where we don't want to
initialize Glean more than once but still want to send pings to a local
server for validation.

Example usage:

```
// Add the following lines to you test class.
@get:Rule
val gleanRule = GleanTestLocalServer(3785)
```

### Parameters

`localPort` - the port of the local ping server