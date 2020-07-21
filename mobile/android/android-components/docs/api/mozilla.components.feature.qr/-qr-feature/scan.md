[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFeature](index.md) / [scan](./scan.md)

# scan

`fun scan(containerViewId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = android.R.id.content): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFeature.kt#L78)

Starts the QR scanner fragment and listens for scan results.

### Parameters

`containerViewId` - optional id of the container this fragment is to
be placed in, defaults to [android.R.id.content](#).

**Return**
true if the scanner was started or false if permissions still
need to be requested.

