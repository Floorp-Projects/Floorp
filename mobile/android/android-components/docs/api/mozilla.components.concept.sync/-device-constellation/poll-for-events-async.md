[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [pollForEventsAsync](./poll-for-events-async.md)

# pollForEventsAsync

`abstract fun pollForEventsAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L93)

Polls for any pending [DeviceEvent](../-device-event/index.md) events.
In case of new events, registered [DeviceEventsObserver](../-device-events-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

