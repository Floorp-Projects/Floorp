[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [SignInWithShareableAccountResult](./index.md)

# SignInWithShareableAccountResult

`enum class SignInWithShareableAccountResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L101)

Describes a result of running [FxaAccountManager.signInWithShareableAccountAsync](../-fxa-account-manager/sign-in-with-shareable-account-async.md).

### Enum Values

| Name | Summary |
|---|---|
| [WillRetry](-will-retry.md) | Sign-in failed due to an intermittent problem (such as a network failure). A retry attempt will be performed automatically during account manager initialization, or as a side-effect of certain user actions (e.g. triggering a sync). |
| [Success](-success.md) | Sign-in succeeded with no issues. Applications may treat this account as "authenticated" after seeing this result. |
| [Failure](-failure.md) | Sign-in failed due to non-recoverable issues. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
