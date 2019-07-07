[android-components](../../index.md) / [mozilla.components.support.locale](../index.md) / [LocaleManager](./index.md)

# LocaleManager

`object LocaleManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/locale/src/main/java/mozilla/components/support/locale/LocaleManager.kt#L18)

Helper for apps that want to change locale defined by the system.

### Functions

| Name | Summary |
|---|---|
| [getCurrentLocale](get-current-locale.md) | `fun getCurrentLocale(context: <ERROR CLASS>): `[`Locale`](https://developer.android.com/reference/java/util/Locale.html)`?`<br>The latest stored locale saved by [setNewLocale](set-new-locale.md). |
| [setNewLocale](set-new-locale.md) | `fun setNewLocale(context: <ERROR CLASS>, language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>`<br>Change the system defined locale to the indicated in the [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) parameter. This new [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) will be stored and will be the new current locale returned by [getCurrentLocale](get-current-locale.md). |
