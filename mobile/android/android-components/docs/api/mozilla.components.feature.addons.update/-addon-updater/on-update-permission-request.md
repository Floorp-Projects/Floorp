[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [AddonUpdater](index.md) / [onUpdatePermissionRequest](./on-update-permission-request.md)

# onUpdatePermissionRequest

`abstract fun onUpdatePermissionRequest(current: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L83)

Invoked when a web extension has changed its permissions while trying to update to a
new version. This requires user interaction as the updated extension will not be installed,
until the user grants the new permissions.

### Parameters

`current` - The current [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md).

`updated` - The updated [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) that requires extra permissions.

`newPermissions` - Contains a list of all the new permissions.

`onPermissionsGranted` - A callback to indicate if the new permissions from the [updated](on-update-permission-request.md#mozilla.components.feature.addons.update.AddonUpdater$onUpdatePermissionRequest(mozilla.components.concept.engine.webextension.WebExtension, mozilla.components.concept.engine.webextension.WebExtension, kotlin.collections.List((kotlin.String)), kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/updated) extension
are granted or not.