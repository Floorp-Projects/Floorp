[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [AccountStorage](./index.md)

# AccountStorage

`interface AccountStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/AccountStorage.kt#L14)

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `abstract fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [read](read.md) | `abstract fun read(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?` |
| [write](write.md) | `abstract fun write(accountState: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SharedPrefAccountStorage](../-shared-pref-account-storage/index.md) | `class SharedPrefAccountStorage : `[`AccountStorage`](./index.md) |
