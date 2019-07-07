[android-components](../../index.md) / [mozilla.components.support.locale](../index.md) / [LocaleManager](index.md) / [setNewLocale](./set-new-locale.md)

# setNewLocale

`fun setNewLocale(context: <ERROR CLASS>, language: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/locale/src/main/java/mozilla/components/support/locale/LocaleManager.kt#L31)

Change the system defined locale to the indicated in the [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) parameter.
This new [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language) will be stored and will be the new current locale returned by [getCurrentLocale](get-current-locale.md).

After calling this function, to visualize the locale changes you have to make sure all your visible activities
get recreated. If your app is using the single activity approach, this will be trivial just call
[AppCompatActivity.recreate](#). On the other hand, if you have multiple activity this could be tricky, one
alternative could be restarting your application process see https://github.com/JakeWharton/ProcessPhoenix

**Return**
A new Context object for whose resources are adjusted to match the new [language](set-new-locale.md#mozilla.components.support.locale.LocaleManager$setNewLocale(, kotlin.String)/language).

