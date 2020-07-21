[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaAccountManager(context: <ERROR CLASS>, serverConfig: `[`ServerConfig`](../../mozilla.components.service.fxa/-server-config.md)`, deviceConfig: `[`DeviceConfig`](../../mozilla.components.service.fxa/-device-config/index.md)`, syncConfig: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`?, applicationScopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptySet(), crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors
        .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob())`

An account manager which encapsulates various internal details of an account lifecycle and provides
an observer interface along with a public API for interacting with an account.
The internal state machine abstracts over state space as exposed by the fxaclient library, not
the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.

Class is 'open' to facilitate testing.

### Parameters

`context` - A [Context](#) instance that's used for internal messaging and interacting with local storage.

`serverConfig` - A [ServerConfig](../../mozilla.components.service.fxa/-server-config.md) used for account initialization.

`deviceConfig` - A description of the current device (name, type, capabilities).

`syncConfig` - Optional, initial sync behaviour configuration. Sync will be disabled if this is `null`.

`applicationScopes` - A set of scopes which will be requested during account authentication.