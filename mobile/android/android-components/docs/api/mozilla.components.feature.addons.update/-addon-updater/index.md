[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [AddonUpdater](./index.md)

# AddonUpdater

`interface AddonUpdater` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L53)

Contract to define the behavior for updating addons.

### Types

| Name | Summary |
|---|---|
| [Frequency](-frequency/index.md) | `class Frequency`<br>Indicates how often an extension should be updated. |
| [Status](-status/index.md) | `sealed class Status`<br>Indicates the status of a request for updating an addon. |
| [UpdateAttempt](-update-attempt/index.md) | `data class UpdateAttempt`<br>Represents an attempt to update an add-on. |

### Functions

| Name | Summary |
|---|---|
| [onUpdatePermissionRequest](on-update-permission-request.md) | `abstract fun onUpdatePermissionRequest(current: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web extension has changed its permissions while trying to update to a new version. This requires user interaction as the updated extension will not be installed, until the user grants the new permissions. |
| [registerForFutureUpdates](register-for-future-updates.md) | `abstract fun registerForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given [addonId](register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.String)/addonId) for periodically check for new updates.`open fun registerForFutureUpdates(extensions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the [extensions](register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.collections.List((mozilla.components.concept.engine.webextension.WebExtension)))/extensions) for periodic updates, if applicable. Built-in and unsupported extensions will not update automatically. |
| [unregisterForFutureUpdates](unregister-for-future-updates.md) | `abstract fun unregisterForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters the given [addonId](unregister-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$unregisterForFutureUpdates(kotlin.String)/addonId) for periodically checking for new updates. |
| [update](update.md) | `abstract fun update(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Try to perform an update on the given [addonId](update.md#mozilla.components.feature.addons.update.AddonUpdater$update(kotlin.String)/addonId). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultAddonUpdater](../-default-addon-updater/index.md) | `class DefaultAddonUpdater : `[`AddonUpdater`](./index.md)<br>An implementation of [AddonUpdater](./index.md) that uses the work manager api for scheduling new updates. |
