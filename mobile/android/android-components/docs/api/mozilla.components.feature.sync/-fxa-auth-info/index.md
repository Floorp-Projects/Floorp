[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [FxaAuthInfo](./index.md)

# FxaAuthInfo

`data class FxaAuthInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/Types.kt#L33)

A Firefox Sync friendly auth object which can be obtained from [FirefoxAccount](../../mozilla.components.service.fxa/-firefox-account/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaAuthInfo(kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A Firefox Sync friendly auth object which can be obtained from [FirefoxAccount](../../mozilla.components.service.fxa/-firefox-account/index.md). |

### Properties

| Name | Summary |
|---|---|
| [fxaAccessToken](fxa-access-token.md) | `val fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [kid](kid.md) | `val kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [syncKey](sync-key.md) | `val syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tokenServerUrl](token-server-url.md) | `val tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
