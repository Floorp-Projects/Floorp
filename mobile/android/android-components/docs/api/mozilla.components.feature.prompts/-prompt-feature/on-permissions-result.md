[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onPermissionsResult](./on-permissions-result.md)

# onPermissionsResult

`fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L165)

Overrides [PermissionsFeature.onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md)

Notifies the feature that the permissions request was completed. It will then
either process or dismiss the prompt request.

### Parameters

`permissions` - List of permission requested.

`grantResults` - The grant results for the corresponding permissions

**See Also**

[onNeedToRequestPermissions](on-need-to-request-permissions.md)

