[android-components](../../index.md) / [mozilla.components.feature.sendtab](../index.md) / [SendTabFeature](./index.md)

# SendTabFeature

`class SendTabFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sendtab/src/main/java/mozilla/components/feature/sendtab/SendTabFeature.kt#L43)

A feature that uses the [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) to send and receive tabs with optional push support
for receiving tabs from the [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) and a [PushService](../../mozilla.components.concept.push/-push-service/index.md).

If the push components are not used, the feature can still function while tabs would only be
received when refreshing the device state.

### Parameters

`accountManager` - Firefox account manager.

`pushFeature` - The [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) if that is setup for observing push events.

`owner` - Android lifecycle owner for the observers. Defaults to the [ProcessLifecycleOwner](#)
so that we can always observe events throughout the application lifecycle.

`autoPause` - whether or not the observer should automatically be
paused/resumed with the bound lifecycle.

`onTabsReceived` - the callback invoked with new tab(s) are received.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SendTabFeature(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, pushFeature: `[`AutoPushFeature`](../../mozilla.components.feature.push/-auto-push-feature/index.md)`? = null, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onTabsReceived: (`[`Device`](../../mozilla.components.concept.sync/-device/index.md)`?, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabData`](../../mozilla.components.concept.sync/-tab-data/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A feature that uses the [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) to send and receive tabs with optional push support for receiving tabs from the [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) and a [PushService](../../mozilla.components.concept.push/-push-service/index.md). |
