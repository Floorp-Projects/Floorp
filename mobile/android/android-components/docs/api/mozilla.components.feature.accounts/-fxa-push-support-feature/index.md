[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FxaPushSupportFeature](./index.md)

# FxaPushSupportFeature

`class FxaPushSupportFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts/src/main/java/mozilla/components/feature/accounts/FxaPushSupportFeature.kt#L38)

A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies
the device during a sync, that it's unable to reach the device via push messaging; triggering a push
registration renewal.

### Parameters

`context` - The application Android context.

`accountManager` - The FxaAccountManager.

`push` - The push processor instance that needs to be notified.

`owner` - the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner](#).

`autoPause` - whether to stop notifying the observer during onPause lifecycle events.
Defaults to false so that observers are always notified.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaPushSupportFeature(context: <ERROR CLASS>, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, push: `[`PushProcessor`](../../mozilla.components.concept.push/-push-processor/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies the device during a sync, that it's unable to reach the device via push messaging; triggering a push registration renewal. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
