[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaAuthData](./index.md)

# FxaAuthData

`data class FxaAuthData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Types.kt#L44)

Captures basic OAuth authentication data (code, state) and any additional data FxA passes along.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaAuthData(authType: `[`AuthType`](../../mozilla.components.concept.sync/-auth-type/index.md)`, code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, declinedEngines: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`SyncEngine`](../-sync-engine/index.md)`>? = null)`<br>Captures basic OAuth authentication data (code, state) and any additional data FxA passes along. |

### Properties

| Name | Summary |
|---|---|
| [authType](auth-type.md) | `val authType: `[`AuthType`](../../mozilla.components.concept.sync/-auth-type/index.md)<br>Type of authentication which caused this object to be created. |
| [code](code.md) | `val code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>OAuth code. |
| [declinedEngines](declined-engines.md) | `val declinedEngines: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`SyncEngine`](../-sync-engine/index.md)`>?`<br>An optional list of [SyncEngine](../-sync-engine/index.md)s that user declined to sync. |
| [state](state.md) | `val state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>OAuth state. |
