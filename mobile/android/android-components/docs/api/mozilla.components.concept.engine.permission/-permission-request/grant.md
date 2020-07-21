[android-components](../../index.md) / [mozilla.components.concept.engine.permission](../index.md) / [PermissionRequest](index.md) / [grant](./grant.md)

# grant

`abstract fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../-permission/index.md)`> = this.permissions): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/permission/PermissionRequest.kt#L28)

Grants the provided permissions, or all requested permissions, if none
are provided.

### Parameters

`permissions` - the permissions to grant.