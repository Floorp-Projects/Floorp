[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [floatPreference](./float-preference.md)

# floatPreference

`fun floatPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L117)

Property delegate for getting and setting a float number shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    var percentage by floatPreference("percentage", default = 0f)
}
```

