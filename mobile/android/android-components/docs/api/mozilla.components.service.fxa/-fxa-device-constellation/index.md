[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](./index.md)

# FxaDeviceConstellation

`class FxaDeviceConstellation : `[`DeviceConstellation`](../../mozilla.components.concept.sync/-device-constellation/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountEventsObserver`](../../mozilla.components.concept.sync/-account-events-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L35)

Provides an implementation of [DeviceConstellation](../../mozilla.components.concept.sync/-device-constellation/index.md) backed by a [FirefoxAccount](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaDeviceConstellation(account: FirefoxAccount, scope: CoroutineScope, crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null)`<br>Provides an implementation of [DeviceConstellation](../../mozilla.components.concept.sync/-device-constellation/index.md) backed by a [FirefoxAccount](#). |

### Functions

| Name | Summary |
|---|---|
| [ensureCapabilitiesAsync](ensure-capabilities-async.md) | `fun ensureCapabilitiesAsync(capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Ensure that all passed in [capabilities](../../mozilla.components.concept.sync/-device-constellation/ensure-capabilities-async.md#mozilla.components.concept.sync.DeviceConstellation$ensureCapabilitiesAsync(kotlin.collections.Set((mozilla.components.concept.sync.DeviceCapability)))/capabilities) are configured. This may involve backend service registration, or other work involving network/disc access. |
| [initDeviceAsync](init-device-async.md) | `fun initDeviceAsync(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)`, capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Register current device in the associated [DeviceConstellation](../../mozilla.components.concept.sync/-device-constellation/index.md). |
| [pollForCommandsAsync](poll-for-commands-async.md) | `fun pollForCommandsAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Polls for any pending [DeviceCommandIncoming](../../mozilla.components.concept.sync/-device-command-incoming/index.md) commands. In case of new commands, registered [AccountEventsObserver](../../mozilla.components.concept.sync/-account-events-observer/index.md) observers will be notified. |
| [processRawEventAsync](process-raw-event-async.md) | `fun processRawEventAsync(payload: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Process a raw event, obtained via a push message or some other out-of-band mechanism. |
| [refreshDevicesAsync](refresh-devices-async.md) | `fun refreshDevicesAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Refreshes [ConstellationState](../../mozilla.components.concept.sync/-constellation-state/index.md). Registered [DeviceConstellationObserver](../../mozilla.components.concept.sync/-device-constellation-observer/index.md) observers will be notified. |
| [registerDeviceObserver](register-device-observer.md) | `fun registerDeviceObserver(observer: `[`DeviceConstellationObserver`](../../mozilla.components.concept.sync/-device-constellation-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Allows monitoring state of the device constellation via [DeviceConstellationObserver](../../mozilla.components.concept.sync/-device-constellation-observer/index.md). Use this to be notified of changes to the current device or other devices. |
| [sendCommandToDeviceAsync](send-command-to-device-async.md) | `fun sendCommandToDeviceAsync(targetDeviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, outgoingCommand: `[`DeviceCommandOutgoing`](../../mozilla.components.concept.sync/-device-command-outgoing/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Send a command to a specified device. |
| [setDeviceNameAsync](set-device-name-async.md) | `fun setDeviceNameAsync(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, context: <ERROR CLASS>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Set name of the current device. |
| [setDevicePushSubscriptionAsync](set-device-push-subscription-async.md) | `fun setDevicePushSubscriptionAsync(subscription: `[`DevicePushSubscription`](../../mozilla.components.concept.sync/-device-push-subscription/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Set a [DevicePushSubscription](../../mozilla.components.concept.sync/-device-push-subscription/index.md) for the current device. |
| [state](state.md) | `fun state(): `[`ConstellationState`](../../mozilla.components.concept.sync/-constellation-state/index.md)`?`<br>Current state of the constellation. May be missing if state was never queried. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
