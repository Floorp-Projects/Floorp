[android-components](../index.md) / [mozilla.components.service.fxa](./index.md)

## Package mozilla.components.service.fxa

### Types

| Name | Summary |
|---|---|
| [AccountStorage](-account-storage/index.md) | `interface AccountStorage` |
| [FirefoxAccount](-firefox-account/index.md) | `class FirefoxAccount : `[`OAuthAccount`](../mozilla.components.concept.sync/-o-auth-account/index.md)<br>FirefoxAccount represents the authentication state of a client. |
| [FxaDeviceConstellation](-fxa-device-constellation/index.md) | `class FxaDeviceConstellation : `[`DeviceConstellation`](../mozilla.components.concept.sync/-device-constellation/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`DeviceEventsObserver`](../mozilla.components.concept.sync/-device-events-observer/index.md)`>`<br>Provides an implementation of [DeviceConstellation](../mozilla.components.concept.sync/-device-constellation/index.md) backed by a [FirefoxAccount](#). |
| [SharedPrefAccountStorage](-shared-pref-account-storage/index.md) | `class SharedPrefAccountStorage : `[`AccountStorage`](-account-storage/index.md) |

### Type Aliases

| Name | Summary |
|---|---|
| [Config](-config.md) | `typealias Config = Config` |
| [FxaException](-fxa-exception.md) | `typealias FxaException = FxaException`<br>High-level exception class for the exceptions thrown in the Rust library. |
| [FxaNetworkException](-fxa-network-exception.md) | `typealias FxaNetworkException = Network`<br>Thrown on a network error. |
| [FxaPanicException](-fxa-panic-exception.md) | `typealias FxaPanicException = Panic`<br>Thrown when the Rust library hits an assertion or panic (this is always a bug). |
| [FxaUnauthorizedException](-fxa-unauthorized-exception.md) | `typealias FxaUnauthorizedException = Unauthorized`<br>Thrown when the operation requires additional authorization. |
| [FxaUnspecifiedException](-fxa-unspecified-exception.md) | `typealias FxaUnspecifiedException = Unspecified`<br>Thrown when the Rust library hits an unexpected error that isn't a panic. This may indicate library misuse, network errors, etc. |
| [PersistCallback](-persist-callback.md) | `typealias PersistCallback = PersistCallback` |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [mozilla.appservices.fxaclient.AccessTokenInfo](mozilla.appservices.fxaclient.-access-token-info/index.md) |  |
| [mozilla.appservices.fxaclient.AccountEvent.TabReceived](mozilla.appservices.fxaclient.-account-event.-tab-received/index.md) |  |
| [mozilla.appservices.fxaclient.Device](mozilla.appservices.fxaclient.-device/index.md) |  |
| [mozilla.appservices.fxaclient.Device.Capability](mozilla.appservices.fxaclient.-device.-capability/index.md) |  |
| [mozilla.appservices.fxaclient.Device.PushSubscription](mozilla.appservices.fxaclient.-device.-push-subscription/index.md) |  |
| [mozilla.appservices.fxaclient.Device.Type](mozilla.appservices.fxaclient.-device.-type/index.md) |  |
| [mozilla.appservices.fxaclient.Profile](mozilla.appservices.fxaclient.-profile/index.md) |  |
| [mozilla.appservices.fxaclient.ScopedKey](mozilla.appservices.fxaclient.-scoped-key/index.md) |  |
| [mozilla.appservices.fxaclient.TabHistoryEntry](mozilla.appservices.fxaclient.-tab-history-entry/index.md) |  |

### Properties

| Name | Summary |
|---|---|
| [FXA_STATE_KEY](-f-x-a_-s-t-a-t-e_-k-e-y.md) | `const val FXA_STATE_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FXA_STATE_PREFS_KEY](-f-x-a_-s-t-a-t-e_-p-r-e-f-s_-k-e-y.md) | `const val FXA_STATE_PREFS_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [into](into.md) | `fun `[`DeviceType`](../mozilla.components.concept.sync/-device-type/index.md)`.into(): Type`<br>`fun `[`DeviceCapability`](../mozilla.components.concept.sync/-device-capability/index.md)`.into(): Capability`<br>`fun `[`DevicePushSubscription`](../mozilla.components.concept.sync/-device-push-subscription/index.md)`.into(): PushSubscription`<br>`fun `[`Device`](../mozilla.components.concept.sync/-device/index.md)`.into(): Device`<br>`fun `[`TabData`](../mozilla.components.concept.sync/-tab-data/index.md)`.into(): TabHistoryEntry`<br>`fun `[`TabReceived`](../mozilla.components.concept.sync/-device-event/-tab-received/index.md)`.into(): TabReceived` |
| [maybeExceptional](maybe-exceptional.md) | `fun <T> maybeExceptional(block: () -> `[`T`](maybe-exceptional.md#T)`, handleErrorBlock: (`[`FxaException`](-fxa-exception.md)`) -> `[`T`](maybe-exceptional.md#T)`): `[`T`](maybe-exceptional.md#T)<br>Runs a provided lambda, and if that throws non-panic FxA exception, runs the second lambda. |
