[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaAccountManager(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, config: `[`Config`](../../mozilla.components.service.fxa/-config.md)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, deviceTuple: `[`DeviceTuple`](../-device-tuple/index.md)`, syncManager: `[`SyncManager`](../../mozilla.components.concept.sync/-sync-manager/index.md)`? = null)`

An account manager which encapsulates various internal details of an account lifecycle and provides
an observer interface along with a public API for interacting with an account.
The internal state machine abstracts over state space as exposed by the fxaclient library, not
the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.

Class is 'open' to facilitate testing.

### Parameters

`context` - A [Context](https://developer.android.com/reference/android/content/Context.html) instance that's used for internal messaging and interacting with local storage.

`config` - A [Config](../../mozilla.components.service.fxa/-config.md) used for account initialization.

`scopes` - A list of scopes which will be requested during account authentication.

`deviceTuple` - A description of the current device (name, type, capabilities).