[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [AddonUpdater](./index.md)

# AddonUpdater

`interface AddonUpdater` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L26)

Contract to define the behavior for updating addons.

### Types

| Name | Summary |
|---|---|
| [Frequency](-frequency/index.md) | `class Frequency`<br>Indicates how often an extension should be updated. |
| [Status](-status/index.md) | `sealed class Status`<br>Indicates the status of a request for updating an addon. |

### Functions

| Name | Summary |
|---|---|
| [registerForFutureUpdates](register-for-future-updates.md) | `abstract fun registerForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given [addonId](register-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$registerForFutureUpdates(kotlin.String)/addonId) for periodically check for new updates. |
| [unregisterForFutureUpdates](unregister-for-future-updates.md) | `abstract fun unregisterForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters the given [addonId](unregister-for-future-updates.md#mozilla.components.feature.addons.update.AddonUpdater$unregisterForFutureUpdates(kotlin.String)/addonId) for periodically checking for new updates. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultAddonUpdater](../-default-addon-updater/index.md) | `class DefaultAddonUpdater : `[`AddonUpdater`](./index.md)<br>An implementation of [AddonUpdater](./index.md) that uses the work manager api for scheduling new updates. |
