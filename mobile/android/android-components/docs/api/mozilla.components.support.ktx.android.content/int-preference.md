[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [intPreference](./int-preference.md)

# intPreference

`fun intPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L131)

Property delegate for getting and setting an int number shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    var widgetNumInvocations by intPreference("widget_number_of_invocations", default = 0)
}
```

