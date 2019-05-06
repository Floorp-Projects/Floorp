[android-components](../../index.md) / [mozilla.components.concept.engine.permission](../index.md) / [PermissionRequest](index.md) / [grantIf](./grant-if.md)

# grantIf

`open fun grantIf(predicate: (`[`Permission`](../-permission/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/permission/PermissionRequest.kt#L37)

Grants this permission request if the provided predicate is true
for any of the requested permissions.

### Parameters

`predicate` - predicate to test for.

**Return**
true if the permission request was granted, otherwise false.

