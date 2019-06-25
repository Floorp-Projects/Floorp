[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SharedPrefAccountStorage](./index.md)

# SharedPrefAccountStorage

`class SharedPrefAccountStorage : `[`AccountStorage`](../-account-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/AccountStorage.kt#L21)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SharedPrefAccountStorage(context: <ERROR CLASS>)` |

### Properties

| Name | Summary |
|---|---|
| [context](context.md) | `val context: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [read](read.md) | `fun read(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?` |
| [write](write.md) | `fun write(accountState: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
