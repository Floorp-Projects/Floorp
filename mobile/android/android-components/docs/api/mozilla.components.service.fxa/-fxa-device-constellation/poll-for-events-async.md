[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [pollForEventsAsync](./poll-for-events-async.md)

# pollForEventsAsync

`fun pollForEventsAsync(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`DeviceEvent`](../../mozilla.components.concept.sync/-device-event/index.md)`>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L90)

Overrides [DeviceConstellation.pollForEventsAsync](../../mozilla.components.concept.sync/-device-constellation/poll-for-events-async.md)

Poll for events targeted at the current [Device](../../mozilla.components.concept.sync/-device/index.md). It's expected that if a [DeviceEvent](../../mozilla.components.concept.sync/-device-event/index.md) was
returned after a poll, it will not be returned in consequent polls.

**Return**
A list of [DeviceEvent](../../mozilla.components.concept.sync/-device-event/index.md) instances that are currently pending for this [Device](../../mozilla.components.concept.sync/-device/index.md), or `null` on failure.

