[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [processRawEvent](./process-raw-event.md)

# processRawEvent

`fun processRawEvent(payload: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L68)

Overrides [DeviceConstellation.processRawEvent](../../mozilla.components.concept.sync/-device-constellation/process-raw-event.md)

Process a raw event, obtained via a push message or some other out-of-band mechanism.

### Parameters

`payload` - A raw, plaintext payload to be processed.