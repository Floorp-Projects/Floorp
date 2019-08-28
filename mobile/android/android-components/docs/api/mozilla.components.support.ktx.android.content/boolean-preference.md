[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [booleanPreference](./boolean-preference.md)

# booleanPreference

`fun booleanPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L103)

Property delegate for getting and setting a boolean shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    val isTelemetryOn by booleanPreference("telemetry", default = false)
}
```

