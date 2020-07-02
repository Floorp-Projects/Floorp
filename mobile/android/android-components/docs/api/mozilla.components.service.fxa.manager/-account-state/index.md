[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [AccountState](./index.md)

# AccountState

`enum class AccountState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/State.kt#L13)

States of the [FxaAccountManager](../-fxa-account-manager/index.md).

### Enum Values

| Name | Summary |
|---|---|
| [Start](-start.md) |  |
| [NotAuthenticated](-not-authenticated.md) |  |
| [AuthenticationProblem](-authentication-problem.md) |  |
| [CanAutoRetryAuthenticationViaTokenCopy](-can-auto-retry-authentication-via-token-copy.md) |  |
| [CanAutoRetryAuthenticationViaTokenReuse](-can-auto-retry-authentication-via-token-reuse.md) |  |
| [AuthenticatedNoProfile](-authenticated-no-profile.md) |  |
| [AuthenticatedWithProfile](-authenticated-with-profile.md) |  |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
