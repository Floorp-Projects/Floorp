[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.integration](../index.md) / [LocaleSettingUpdater](./index.md)

# LocaleSettingUpdater

`class LocaleSettingUpdater : `[`SettingUpdater`](../-setting-updater/index.md)`<`[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/integration/LocaleSettingUpdater.kt#L17)

Class to set the locales setting for geckoview, updating from the locale of the device.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LocaleSettingUpdater(context: <ERROR CLASS>, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)`)`<br>Class to set the locales setting for geckoview, updating from the locale of the device. |

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `var value: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>The setter for this property should change the GeckoView setting. |

### Inherited Properties

| Name | Summary |
|---|---|
| [enabled](../-setting-updater/enabled.md) | `var enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Toggle the automatic tracking of a setting derived from the device state. |

### Functions

| Name | Summary |
|---|---|
| [findValue](find-value.md) | `fun findValue(): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Find the value of the setting from the device state. This is setting specific. |
| [registerForUpdates](register-for-updates.md) | `fun registerForUpdates(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Register for updates from the device state. This is setting specific. |
| [unregisterForUpdates](unregister-for-updates.md) | `fun unregisterForUpdates(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregister for updates from the device state. |
