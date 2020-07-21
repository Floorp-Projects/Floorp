[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFeature](index.md) / [onPermissionsResult](./on-permissions-result.md)

# onPermissionsResult

`fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFeature.kt#L96)

Overrides [PermissionsFeature.onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md)

Notifies the feature that the permission request was completed. If the
requested permissions were granted it will open the QR scanner.

