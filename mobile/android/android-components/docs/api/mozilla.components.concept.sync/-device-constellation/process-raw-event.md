[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [processRawEvent](./process-raw-event.md)

# processRawEvent

`abstract fun processRawEvent(payload: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L80)

Process a raw event, obtained via a push message or some other out-of-band mechanism.

### Parameters

`payload` - A raw, plaintext payload to be processed.