[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [processRawEventAsync](./process-raw-event-async.md)

# processRawEventAsync

`abstract fun processRawEventAsync(payload: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L84)

Process a raw event, obtained via a push message or some other out-of-band mechanism.

### Parameters

`payload` - A raw, plaintext payload to be processed.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

