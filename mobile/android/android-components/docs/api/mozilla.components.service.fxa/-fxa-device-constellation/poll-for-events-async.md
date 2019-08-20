[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [pollForEventsAsync](./poll-for-events-async.md)

# pollForEventsAsync

`fun pollForEventsAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L119)

Overrides [DeviceConstellation.pollForEventsAsync](../../mozilla.components.concept.sync/-device-constellation/poll-for-events-async.md)

Polls for any pending [DeviceEvent](../../mozilla.components.concept.sync/-device-event/index.md) events.
In case of new events, registered [DeviceEventsObserver](../../mozilla.components.concept.sync/-device-events-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

