[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [Addon](index.md) / [localizePermissions](./localize-permissions.md)

# localizePermissions

`fun localizePermissions(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/Addon.kt#L196)

Takes a list of [permissions](localize-permissions.md#mozilla.components.feature.addons.Addon.Companion$localizePermissions(kotlin.collections.List((kotlin.String)))/permissions) and returns a list of id resources per each item.

### Parameters

`permissions` - The list of permissions to be localized. Valid permissions can be found in
https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#API_permissions