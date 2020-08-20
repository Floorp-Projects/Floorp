[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [pollForCommandsAsync](./poll-for-commands-async.md)

# pollForCommandsAsync

`fun pollForCommandsAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L127)

Overrides [DeviceConstellation.pollForCommandsAsync](../../mozilla.components.concept.sync/-device-constellation/poll-for-commands-async.md)

Polls for any pending [DeviceCommandIncoming](../../mozilla.components.concept.sync/-device-command-incoming/index.md) commands.
In case of new commands, registered [AccountEventsObserver](../../mozilla.components.concept.sync/-account-events-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

