[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [longPreference](./long-preference.md)

# longPreference

`fun longPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L145)

Property delegate for getting and setting a long number shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    val appInstanceId by longPreference("app_instance_id", default = 123456789L)
}
```

