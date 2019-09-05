[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [stringPreference](./string-preference.md)

# stringPreference

`fun stringPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L159)

Property delegate for getting and setting a string shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    var permissionsEnabledEnum by stringPreference("permissions_enabled", default = "blocked")
}
```

