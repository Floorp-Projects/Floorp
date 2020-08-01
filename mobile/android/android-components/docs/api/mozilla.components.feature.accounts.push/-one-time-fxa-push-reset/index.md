[android-components](../../index.md) / [mozilla.components.feature.accounts.push](../index.md) / [OneTimeFxaPushReset](./index.md)

# OneTimeFxaPushReset

`class OneTimeFxaPushReset` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts-push/src/main/java/mozilla/components/feature/accounts/push/FxaPushSupportFeature.kt#L309)

Resets the fxa push scope (and therefore push subscription) if it does not follow the new format.

This is needed only for our existing push users and can be removed when we're more confident our users are
all migrated.

Implementation Notes: In order to support a new performance fix related to push and
[FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) we need to use a new push scope format. This class checks if we have the old
format, and removes it if so, thereby generating a new push scope with the new format.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `OneTimeFxaPushReset(context: <ERROR CLASS>, pushFeature: `[`AutoPushFeature`](../../mozilla.components.feature.push/-auto-push-feature/index.md)`)`<br>Resets the fxa push scope (and therefore push subscription) if it does not follow the new format. |

### Functions

| Name | Summary |
|---|---|
| [resetSubscriptionIfNeeded](reset-subscription-if-needed.md) | `fun resetSubscriptionIfNeeded(account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resets the push subscription if the old subscription format is used. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
