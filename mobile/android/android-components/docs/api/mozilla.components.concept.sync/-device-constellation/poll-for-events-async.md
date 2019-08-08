[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [pollForEventsAsync](./poll-for-events-async.md)

# pollForEventsAsync

`abstract fun pollForEventsAsync(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`DeviceEvent`](../-device-event/index.md)`>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L91)

Poll for events targeted at the current [Device](../-device/index.md). It's expected that if a [DeviceEvent](../-device-event/index.md) was
returned after a poll, it will not be returned in consequent polls.

**Return**
A list of [DeviceEvent](../-device-event/index.md) instances that are currently pending for this [Device](../-device/index.md), or `null` on failure.

