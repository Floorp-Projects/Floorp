[android-components](../../index.md) / [mozilla.components.service.glean.testing](../index.md) / [GleanTestLocalServer](./index.md)

# GleanTestLocalServer

`class GleanTestLocalServer : TestWatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/testing/GleanTestLocalServer.kt#L33)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GleanTestLocalServer(localPort: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>This implements a JUnit rule for writing tests for Glean SDK metrics. |

### Functions

| Name | Summary |
|---|---|
| [starting](starting.md) | `fun starting(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
