[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [SecureAbove22Preferences](./index.md)

# SecureAbove22Preferences

`class SecureAbove22Preferences : KeyValuePreferences` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/SecureAbove22Preferences.kt#L56)

A wrapper around [SharedPreferences](#) which encrypts contents on supported API versions (23+).
Otherwise, this simply delegates to [SharedPreferences](#).

In rare circumstances (such as APK signing key rotation) a master key which protects this storage may be lost,
in which case previously stored values will be lost as well. Applications are encouraged to instrument such events.

### Parameters

`context` - A [Context](#), used for accessing [SharedPreferences](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SecureAbove22Preferences(context: <ERROR CLASS>)`<br>A wrapper around [SharedPreferences](#) which encrypts contents on supported API versions (23+). Otherwise, this simply delegates to [SharedPreferences](#). |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears all key/value pairs from the storage. |
| [getString](get-string.md) | `fun getString(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Retrieves a stored [key](#). See [putString](#) for storing a [key](#). |
| [putString](put-string.md) | `fun putString(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stores [value](#) under [key](#). Retrieve it using [getString](#). |
| [remove](remove.md) | `fun remove(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes key/value pair from storage for the provided [key](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
