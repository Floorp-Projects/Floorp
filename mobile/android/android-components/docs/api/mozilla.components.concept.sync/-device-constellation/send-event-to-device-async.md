[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [sendEventToDeviceAsync](./send-event-to-device-async.md)

# sendEventToDeviceAsync

`abstract fun sendEventToDeviceAsync(targetDeviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, outgoingEvent: `[`DeviceEventOutgoing`](../-device-event-outgoing/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L77)

Send an event to a specified device.

### Parameters

`targetDeviceId` - A device ID of the recipient.

`outgoingEvent` - An event to send.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

