[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccountEvent](./index.md)

# AccountEvent

`sealed class AccountEvent` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/AccountEvent.kt#L20)

Incoming account events.

### Types

| Name | Summary |
|---|---|
| [AccountAuthStateChanged](-account-auth-state-changed/index.md) | `class AccountAuthStateChanged : `[`AccountEvent`](./index.md)<br>The authentication state of the account changed - eg, the password changed |
| [AccountDestroyed](-account-destroyed/index.md) | `class AccountDestroyed : `[`AccountEvent`](./index.md)<br>The account itself was destroyed |
| [DeviceCommandIncoming](-device-command-incoming/index.md) | `class DeviceCommandIncoming : `[`AccountEvent`](./index.md)<br>An incoming command from another device |
| [DeviceConnected](-device-connected/index.md) | `class DeviceConnected : `[`AccountEvent`](./index.md)<br>Another device connected to the account |
| [DeviceDisconnected](-device-disconnected/index.md) | `class DeviceDisconnected : `[`AccountEvent`](./index.md)<br>A device (possibly this one) disconnected from the account |
| [ProfileUpdated](-profile-updated/index.md) | `class ProfileUpdated : `[`AccountEvent`](./index.md)<br>The account's profile was updated |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AccountAuthStateChanged](-account-auth-state-changed/index.md) | `class AccountAuthStateChanged : `[`AccountEvent`](./index.md)<br>The authentication state of the account changed - eg, the password changed |
| [AccountDestroyed](-account-destroyed/index.md) | `class AccountDestroyed : `[`AccountEvent`](./index.md)<br>The account itself was destroyed |
| [DeviceCommandIncoming](-device-command-incoming/index.md) | `class DeviceCommandIncoming : `[`AccountEvent`](./index.md)<br>An incoming command from another device |
| [DeviceConnected](-device-connected/index.md) | `class DeviceConnected : `[`AccountEvent`](./index.md)<br>Another device connected to the account |
| [DeviceDisconnected](-device-disconnected/index.md) | `class DeviceDisconnected : `[`AccountEvent`](./index.md)<br>A device (possibly this one) disconnected from the account |
| [ProfileUpdated](-profile-updated/index.md) | `class ProfileUpdated : `[`AccountEvent`](./index.md)<br>The account's profile was updated |
