[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [SendTabUseCases](./index.md)

# SendTabUseCases

`class SendTabUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts-push/src/main/java/mozilla/components/feature/accounts/push/SendTabUseCases.kt#L34)

Contains use cases for sending tabs to devices related to the firefox-accounts.

See [SendTabFeature](../-send-tab-feature/index.md) for the ability to receive tabs from other devices.

### Parameters

`accountManager` - The AccountManager on which we want to retrieve our devices.

`coroutineContext` - The Coroutine Context on which we want to do the actual sending.
By default, we want to do this on the IO dispatcher since it involves making network requests to
the Sync servers.

### Types

| Name | Summary |
|---|---|
| [SendToAllUseCase](-send-to-all-use-case/index.md) | `class SendToAllUseCase` |
| [SendToDeviceUseCase](-send-to-device-use-case/index.md) | `class SendToDeviceUseCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SendTabUseCases(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>Contains use cases for sending tabs to devices related to the firefox-accounts. |

### Properties

| Name | Summary |
|---|---|
| [sendToAllAsync](send-to-all-async.md) | `val sendToAllAsync: `[`SendToAllUseCase`](-send-to-all-use-case/index.md) |
| [sendToDeviceAsync](send-to-device-async.md) | `val sendToDeviceAsync: `[`SendToDeviceUseCase`](-send-to-device-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
