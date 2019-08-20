[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [shareableAccounts](./shareable-accounts.md)

# shareableAccounts

`fun shareableAccounts(context: <ERROR CLASS>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ShareableAccount`](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L257)

Queries trusted FxA Auth providers available on the device, returning a list of [ShareableAccount](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md)
in an order of preference. Any of the returned [ShareableAccount](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md) may be used with
[signInWithShareableAccountAsync](sign-in-with-shareable-account-async.md) to sign-in into an FxA account without any required user input.

