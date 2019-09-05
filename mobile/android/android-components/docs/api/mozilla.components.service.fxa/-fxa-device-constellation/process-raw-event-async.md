[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [processRawEventAsync](./process-raw-event-async.md)

# processRawEventAsync

`fun processRawEventAsync(payload: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L68)

Overrides [DeviceConstellation.processRawEventAsync](../../mozilla.components.concept.sync/-device-constellation/process-raw-event-async.md)

Process a raw event, obtained via a push message or some other out-of-band mechanism.

### Parameters

`payload` - A raw, plaintext payload to be processed.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

