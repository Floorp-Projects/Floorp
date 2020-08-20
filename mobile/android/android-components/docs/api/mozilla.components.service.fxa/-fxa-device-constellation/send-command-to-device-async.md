[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [sendCommandToDeviceAsync](./send-command-to-device-async.md)

# sendCommandToDeviceAsync

`fun sendCommandToDeviceAsync(targetDeviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, outgoingCommand: `[`DeviceCommandOutgoing`](../../mozilla.components.concept.sync/-device-command-outgoing/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L108)

Overrides [DeviceConstellation.sendCommandToDeviceAsync](../../mozilla.components.concept.sync/-device-constellation/send-command-to-device-async.md)

Send a command to a specified device.

### Parameters

`targetDeviceId` - A device ID of the recipient.

`outgoingCommand` - An event to send.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

