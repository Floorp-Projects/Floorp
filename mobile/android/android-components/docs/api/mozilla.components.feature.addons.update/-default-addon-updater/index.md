[android-components](../../index.md) / [mozilla.components.feature.addons.update](../index.md) / [DefaultAddonUpdater](./index.md)

# DefaultAddonUpdater

`class DefaultAddonUpdater : `[`AddonUpdater`](../-addon-updater/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L80)

An implementation of [AddonUpdater](../-addon-updater/index.md) that uses the work manager api for scheduling new updates.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultAddonUpdater(applicationContext: <ERROR CLASS>, frequency: `[`Frequency`](../-addon-updater/-frequency/index.md)` = Frequency(1, TimeUnit.DAYS))`<br>An implementation of [AddonUpdater](../-addon-updater/index.md) that uses the work manager api for scheduling new updates. |

### Functions

| Name | Summary |
|---|---|
| [registerForFutureUpdates](register-for-future-updates.md) | `fun registerForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.registerForFutureUpdates](../-addon-updater/register-for-future-updates.md) |
| [unregisterForFutureUpdates](unregister-for-future-updates.md) | `fun unregisterForFutureUpdates(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [AddonUpdater.unregisterForFutureUpdates](../-addon-updater/unregister-for-future-updates.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
