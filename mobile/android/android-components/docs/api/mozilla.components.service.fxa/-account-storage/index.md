[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [AccountStorage](./index.md)

# AccountStorage

`interface AccountStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/AccountStorage.kt#L14)

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `abstract fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [read](read.md) | `abstract fun read(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?` |
| [write](write.md) | `abstract fun write(accountState: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [SharedPrefAccountStorage](../-shared-pref-account-storage/index.md) | `class SharedPrefAccountStorage : `[`AccountStorage`](./index.md) |
