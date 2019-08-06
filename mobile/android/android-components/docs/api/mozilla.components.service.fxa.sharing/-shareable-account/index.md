[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [ShareableAccount](./index.md)

# ShareableAccount

`data class ShareableAccount` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L18)

Data structure describing an FxA account within another package that may be used to sign-in.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ShareableAccount(email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, sourcePackage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, authInfo: `[`ShareableAuthInfo`](../-shareable-auth-info/index.md)`)`<br>Data structure describing an FxA account within another package that may be used to sign-in. |

### Properties

| Name | Summary |
|---|---|
| [authInfo](auth-info.md) | `val authInfo: `[`ShareableAuthInfo`](../-shareable-auth-info/index.md) |
| [email](email.md) | `val email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [sourcePackage](source-package.md) | `val sourcePackage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
