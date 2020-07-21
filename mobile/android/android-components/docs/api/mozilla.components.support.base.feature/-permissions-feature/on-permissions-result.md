[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [PermissionsFeature](index.md) / [onPermissionsResult](./on-permissions-result.md)

# onPermissionsResult

`abstract fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/PermissionsFeature.kt#L50)

Notifies the feature that a permission request was completed.
The feature should react to this and complete its task.

### Parameters

`permissions` - The permissions that were granted.

`grantResults` - The grant results for the corresponding permission