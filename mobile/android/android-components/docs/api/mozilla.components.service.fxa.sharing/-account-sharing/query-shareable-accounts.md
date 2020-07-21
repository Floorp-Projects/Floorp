[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [AccountSharing](index.md) / [queryShareableAccounts](./query-shareable-accounts.md)

# queryShareableAccounts

`fun queryShareableAccounts(context: <ERROR CLASS>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ShareableAccount`](../-shareable-account/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L69)

Queries trusted FxA Auth providers present on the device, returning a list of accounts that
can be used for signing in automatically.

**Return**
A list of [ShareableAccount](../-shareable-account/index.md) that are present on the device, in an order of
suggested preference.

