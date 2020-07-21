[android-components](../../index.md) / [mozilla.components.support.locale](../index.md) / [LocaleManager](index.md) / [resetToSystemDefault](./reset-to-system-default.md)

# resetToSystemDefault

`fun resetToSystemDefault(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/locale/src/main/java/mozilla/components/support/locale/LocaleManager.kt#L55)

Change the current locale to the system defined one. As a result, [getCurrentLocale](get-current-locale.md) will
return null.

After calling this function, to visualize the locale changes you have to make sure all your visible activities
get recreated. If your app is using the single activity approach, this will be trivial just call
[AppCompatActivity.recreate](#). On the other hand, if you have multiple activity this could be tricky, one
alternative could be restarting your application process see https://github.com/JakeWharton/ProcessPhoenix

