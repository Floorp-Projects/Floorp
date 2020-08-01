[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko.permission](../../index.md) / [GeckoPermissionRequest](../index.md) / [Media](index.md) / [grant](./grant.md)

# grant

`fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/permission/GeckoPermissionRequest.kt#L107)

Overrides [GeckoPermissionRequest.grant](../grant.md)

Grants the provided permissions, or all requested permissions, if none
are provided.

### Parameters

`permissions` - the permissions to grant.