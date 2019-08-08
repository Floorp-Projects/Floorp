[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [sendEventToDeviceAsync](./send-event-to-device-async.md)

# sendEventToDeviceAsync

`fun sendEventToDeviceAsync(targetDeviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, outgoingEvent: `[`DeviceEventOutgoing`](../../mozilla.components.concept.sync/-device-event-outgoing/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L141)

Overrides [DeviceConstellation.sendEventToDeviceAsync](../../mozilla.components.concept.sync/-device-constellation/send-event-to-device-async.md)

Send an event to a specified device.

### Parameters

`targetDeviceId` - A device ID of the recipient.

`outgoingEvent` - An event to send.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

