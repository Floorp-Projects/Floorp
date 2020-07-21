[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccountObserver](./index.md)

# AccountObserver

`interface AccountObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L312)

Observer interface which lets its users monitor account state changes and major events.
(XXX - there's some tension between this and the
mozilla.components.concept.sync.AccountEvent we should resolve!)

### Functions

| Name | Summary |
|---|---|
| [onAuthenticated](on-authenticated.md) | `open fun onAuthenticated(account: `[`OAuthAccount`](../-o-auth-account/index.md)`, authType: `[`AuthType`](../-auth-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account was successfully authenticated. |
| [onAuthenticationProblems](on-authentication-problems.md) | `open fun onAuthenticationProblems(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account needs to be re-authenticated (e.g. due to a password change). |
| [onLoggedOut](on-logged-out.md) | `open fun onLoggedOut(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account just got logged out. |
| [onProfileUpdated](on-profile-updated.md) | `open fun onProfileUpdated(profile: `[`Profile`](../-profile/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account's profile is now available. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
