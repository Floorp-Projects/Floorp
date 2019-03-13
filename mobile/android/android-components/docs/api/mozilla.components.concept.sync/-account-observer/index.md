[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AccountObserver](./index.md)

# AccountObserver

`interface AccountObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L57)

Observer interface which lets its users monitor account state changes and major events.

### Functions

| Name | Summary |
|---|---|
| [onAuthenticated](on-authenticated.md) | `abstract fun onAuthenticated(account: `[`OAuthAccount`](../-o-auth-account/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account was successfully authenticated. |
| [onError](on-error.md) | `abstract fun onError(error: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account manager encountered an error. Inspect [error](on-error.md#mozilla.components.concept.sync.AccountObserver$onError(java.lang.Exception)/error) for details. |
| [onLoggedOut](on-logged-out.md) | `abstract fun onLoggedOut(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account just got logged out. |
| [onProfileUpdated](on-profile-updated.md) | `abstract fun onProfileUpdated(profile: `[`Profile`](../-profile/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Account's profile is now available. |
