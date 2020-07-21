[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [FxaPushSupportFeature](./index.md)

# FxaPushSupportFeature

`class FxaPushSupportFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts-push/src/main/java/mozilla/components/feature/accounts/push/FxaPushSupportFeature.kt#L46)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaPushSupportFeature(context: <ERROR CLASS>, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, pushFeature: `[`AutoPushFeature`](../../mozilla.components.feature.push/-auto-push-feature/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies the device during a sync, that it's unable to reach the device via push messaging; triggering a push registration renewal. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [PUSH_SCOPE_PREFIX](-p-u-s-h_-s-c-o-p-e_-p-r-e-f-i-x.md) | `const val PUSH_SCOPE_PREFIX: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
