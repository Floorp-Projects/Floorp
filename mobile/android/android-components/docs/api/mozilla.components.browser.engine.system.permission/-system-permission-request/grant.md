[android-components](../../index.md) / [mozilla.components.browser.engine.system.permission](../index.md) / [SystemPermissionRequest](index.md) / [grant](./grant.md)

# grant

`fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/permission/SystemPermissionRequest.kt#L25)

Overrides [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md)

Grants the provided permissions, or all requested permissions, if none
are provided.

### Parameters

`permissions` - the permissions to grant.