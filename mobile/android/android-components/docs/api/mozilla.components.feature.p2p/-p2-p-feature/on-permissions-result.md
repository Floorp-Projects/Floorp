[android-components](../../index.md) / [mozilla.components.feature.p2p](../index.md) / [P2PFeature](index.md) / [onPermissionsResult](./on-permissions-result.md)

# onPermissionsResult

`fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/P2PFeature.kt#L92)

Overrides [PermissionsFeature.onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md)

Notifies the feature that a permission request was completed.
The feature should react to this and complete its task.

### Parameters

`permissions` - The permissions that were granted.

`grantResults` - The grant results for the corresponding permission