[android-components](../../index.md) / [mozilla.components.feature.sendtab](../index.md) / [SendTabUseCases](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SendTabUseCases(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`

Contains use cases for sending tabs to devices related to the firefox-accounts.

### Parameters

`accountManager` - The AccountManager on which we want to retrieve our devices.

`coroutineContext` - The Coroutine Context on which we want to do the actual sending.
By default, we want to do this on the IO dispatcher since it involves making network requests to
the Sync servers.