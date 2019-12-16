[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [SendTabFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SendTabFeature(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onTabsReceived: (`[`Device`](../../mozilla.components.concept.sync/-device/index.md)`?, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabData`](../../mozilla.components.concept.sync/-tab-data/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

A feature that uses the [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) to receive tabs.

Adding push support with [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) will allow for the account to be notified immediately.
If the push components are not used, the feature can still function
while tabs would only be received when refreshing the device state.

See [SendTabUseCases](../-send-tab-use-cases/index.md) for the ability to send tabs to other devices.

### Parameters

`accountManager` - Firefox account manager.

`owner` - Android lifecycle owner for the observers. Defaults to the [ProcessLifecycleOwner](#)
so that we can always observe events throughout the application lifecycle.

`autoPause` - whether or not the observer should automatically be
paused/resumed with the bound lifecycle.

`onTabsReceived` - the callback invoked with new tab(s) are received.