[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FirefoxAccountsAuthFeature](./index.md)

# FirefoxAccountsAuthFeature

`class FirefoxAccountsAuthFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts/src/main/java/mozilla/components/feature/accounts/FirefoxAccountsAuthFeature.kt#L22)

Ties together an account manager with a session manager/tabs implementation, facilitating an
authentication flow.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FirefoxAccountsAuthFeature(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa/-fxa-account-manager/index.md)`, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, redirectUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, successPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.Main)`<br>Ties together an account manager with a session manager/tabs implementation, facilitating an authentication flow. |

### Properties

| Name | Summary |
|---|---|
| [interceptor](interceptor.md) | `val interceptor: `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.md) |

### Functions

| Name | Summary |
|---|---|
| [beginAuthentication](begin-authentication.md) | `fun beginAuthentication(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [beginPairingAuthentication](begin-pairing-authentication.md) | `fun beginPairingAuthentication(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
