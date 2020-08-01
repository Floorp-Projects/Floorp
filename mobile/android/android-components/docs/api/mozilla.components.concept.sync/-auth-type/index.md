[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AuthType](./index.md)

# AuthType

`sealed class AuthType` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L270)

### Types

| Name | Summary |
|---|---|
| [Existing](-existing.md) | `object Existing : `[`AuthType`](./index.md)<br>Account restored from hydrated state on disk. |
| [OtherExternal](-other-external/index.md) | `data class OtherExternal : `[`AuthType`](./index.md)<br>Account was created for an unknown external reason, hopefully identified by [action](-other-external/action.md). |
| [Pairing](-pairing.md) | `object Pairing : `[`AuthType`](./index.md)<br>Account created via pairing (similar to sign-in, but without requiring credentials). |
| [Recovered](-recovered.md) | `object Recovered : `[`AuthType`](./index.md)<br>Existing account was recovered from an authentication problem. |
| [Shared](-shared.md) | `object Shared : `[`AuthType`](./index.md)<br>Account created via a shared account state from another app. |
| [Signin](-signin.md) | `object Signin : `[`AuthType`](./index.md)<br>Account created in response to a sign-in. |
| [Signup](-signup.md) | `object Signup : `[`AuthType`](./index.md)<br>Account created in response to a sign-up. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Existing](-existing.md) | `object Existing : `[`AuthType`](./index.md)<br>Account restored from hydrated state on disk. |
| [OtherExternal](-other-external/index.md) | `data class OtherExternal : `[`AuthType`](./index.md)<br>Account was created for an unknown external reason, hopefully identified by [action](-other-external/action.md). |
| [Pairing](-pairing.md) | `object Pairing : `[`AuthType`](./index.md)<br>Account created via pairing (similar to sign-in, but without requiring credentials). |
| [Recovered](-recovered.md) | `object Recovered : `[`AuthType`](./index.md)<br>Existing account was recovered from an authentication problem. |
| [Shared](-shared.md) | `object Shared : `[`AuthType`](./index.md)<br>Account created via a shared account state from another app. |
| [Signin](-signin.md) | `object Signin : `[`AuthType`](./index.md)<br>Account created in response to a sign-in. |
| [Signup](-signup.md) | `object Signup : `[`AuthType`](./index.md)<br>Account created in response to a sign-up. |
