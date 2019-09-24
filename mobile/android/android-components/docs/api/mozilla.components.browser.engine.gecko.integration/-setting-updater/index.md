[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.integration](../index.md) / [SettingUpdater](./index.md)

# SettingUpdater

`abstract class SettingUpdater<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/integration/SettingUpdater.kt#L7)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SettingUpdater()` |

### Properties

| Name | Summary |
|---|---|
| [enabled](enabled.md) | `var enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Toggle the automatic tracking of a setting derived from the device state. |
| [value](value.md) | `abstract var value: `[`T`](index.md#T)<br>The setter for this property should change the GeckoView setting. |

### Functions

| Name | Summary |
|---|---|
| [findValue](find-value.md) | `abstract fun findValue(): `[`T`](index.md#T)<br>Find the value of the setting from the device state. This is setting specific. |
| [registerForUpdates](register-for-updates.md) | `abstract fun registerForUpdates(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Register for updates from the device state. This is setting specific. |
| [unregisterForUpdates](unregister-for-updates.md) | `abstract fun unregisterForUpdates(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregister for updates from the device state. |

### Inheritors

| Name | Summary |
|---|---|
| [LocaleSettingUpdater](../-locale-setting-updater/index.md) | `class LocaleSettingUpdater : `[`SettingUpdater`](./index.md)`<`[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>`<br>Class to set the locales setting for geckoview, updating from the locale of the device. |
