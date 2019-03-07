[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [handleEvent](./handle-event.md)

# handleEvent

`fun handleEvent(pingEvent: `[`PingEvent`](../-glean/-ping-event/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L258)

Handle an event and send the appropriate pings.

Valid events are:

* Background: When the application goes to the background.
    This triggers sending of the baseline ping.
* Default: Event that triggers the default pings.

### Parameters

`pingEvent` - The type of the event.