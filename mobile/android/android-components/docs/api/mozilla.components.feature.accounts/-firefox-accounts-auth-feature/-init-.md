[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FirefoxAccountsAuthFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FirefoxAccountsAuthFeature(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, redirectUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO, onBeginAuthentication: (<ERROR CLASS>, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> })`

Ties together an account manager with a session manager/tabs implementation, facilitating an
authentication flow.

