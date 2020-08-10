[android-components](../../index.md) / [mozilla.components.support.locale](../index.md) / [LocaleManager](./index.md)

# LocaleManager

`object LocaleManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/locale/src/main/java/mozilla/components/support/locale/LocaleManager.kt#L20)

Helper for apps that want to change locale defined by the system.

### Functions

| Name | Summary |
|---|---|
| [getCurrentLocale](get-current-locale.md) | `fun getCurrentLocale(context: <ERROR CLASS>): `[`Locale`](http://docs.oracle.com/javase/7/docs/api/java/util/Locale.html)`?`<br>The latest stored locale saved by [setNewLocale](set-new-locale.md). |
| [getSystemDefault](get-system-default.md) | `fun getSystemDefault(): `[`Locale`](http://docs.oracle.com/javase/7/docs/api/java/util/Locale.html)<br>Returns the locale set by the system |
| [resetToSystemDefault](reset-to-system-default.md) | `fun resetToSystemDefault(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Change the current locale to the system defined one. As a result, [getCurrentLocale](get-current-locale.md) will return null. |
| [setNewLocale](set-new-locale.md) | `fun setNewLocale(context: <ERROR CLASS>, language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>`<br>Change the system defined locale to the indicated in the [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) parameter. This new [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) will be stored and will be the new current locale returned by [getCurrentLocale](get-current-locale.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
