[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [stringSetPreference](./string-set-preference.md)

# stringSetPreference

`fun stringSetPreference(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`PreferencesHolder`](-preferences-holder/index.md)`, `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/SharedPreferences.kt#L173)

Property delegate for getting and setting a string set shared preference.

Example usage:

```
class Settings : PreferenceHolder {
    ...
    var connectedDevices by stringSetPreference("connected_devices", default = emptySet())
}
```

