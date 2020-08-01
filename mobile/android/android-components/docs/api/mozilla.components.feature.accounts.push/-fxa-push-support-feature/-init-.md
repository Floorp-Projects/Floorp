[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [FxaPushSupportFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaPushSupportFeature(context: <ERROR CLASS>, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, pushFeature: `[`AutoPushFeature`](../../mozilla.components.feature.push/-auto-push-feature/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies
the device during a sync, that it's unable to reach the device via push messaging; triggering a push
registration renewal.

### Parameters

`context` - The application Android context.

`accountManager` - The FxaAccountManager.

`pushFeature` - The [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) if that is setup for observing push events.

`owner` - the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner](#).

`autoPause` - whether to stop notifying the observer during onPause lifecycle events.
Defaults to false so that observers are always notified.