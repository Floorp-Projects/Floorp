[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [pollForCommandsAsync](./poll-for-commands-async.md)

# pollForCommandsAsync

`abstract fun pollForCommandsAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L95)

Polls for any pending [DeviceCommandIncoming](../-device-command-incoming/index.md) commands.
In case of new commands, registered [AccountEventsObserver](../-account-events-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

