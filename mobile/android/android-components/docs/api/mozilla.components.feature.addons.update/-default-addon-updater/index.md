[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [DefaultAddonUpdater](./index.md)

# DefaultAddonUpdater

`class DefaultAddonUpdater : `[`AddonUpdater`](../-addon-updater/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L153)

An implementation of [AddonUpdater](../-addon-updater/index.md) that uses the work manager api for scheduling new updates.

### Types

| Name | Summary |
|---|---|
| [UpdateAttemptStorage](-update-attempt-storage/index.md) | `class UpdateAttemptStorage`<br>A storage implementation to persist [AddonUpdater.UpdateAttempt](../-addon-updater/-update-attempt/index.md)s. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultAddonUpdater(applicationContext: <ERROR CLASS>, frequency: `[`Frequency`](../-addon-updater/-frequency/index.md)` = Frequency(1, TimeUnit.DAYS))`<br>An implementation of [AddonUpdater](../-addon-updater/index.md) that uses the work manager api for scheduling new updates. |

### Functions

| Name | Summary |
|---|---|
| [onUpdatePermissionRequest](on-update-permission-request.md) | `fun onUpdatePermissionRequest(current: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.onUpdatePermissionRequest](../-addon-updater/on-update-permission-request.md) |
| [registerForFutureUpdates](register-for-future-updates.md) | `fun registerForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.registerForFutureUpdates](../-addon-updater/register-for-future-updates.md). If an add-on is already registered nothing will happen. |
| [unregisterForFutureUpdates](unregister-for-future-updates.md) | `fun unregisterForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.unregisterForFutureUpdates](../-addon-updater/unregister-for-future-updates.md) |
| [update](update.md) | `fun update(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.update](../-addon-updater/update.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [registerForFutureUpdates](../-addon-updater/register-for-future-updates.md) | `open fun registerForFutureUpdates(extensions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the [extensions](../-addon-updater/register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.collections.List((mozilla.components.concept.engine.webextension.WebExtension)))/extensions) for periodic updates, if applicable. Built-in and unsupported extensions will not update automatically. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
