[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FxaMigrationResult](../index.md) / [Success](./index.md)

# Success

`sealed class Success : `[`FxaMigrationResult`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L55)

Success variants of an FxA migration.

### Types

| Name | Summary |
|---|---|
| [NoAccount](-no-account.md) | `object NoAccount : `[`Success`](./index.md)<br>No Fennec account found. |
| [SignedInIntoAuthenticatedAccount](-signed-in-into-authenticated-account/index.md) | `data class SignedInIntoAuthenticatedAccount : `[`Success`](./index.md)<br>Successfully signed-in with an authenticated Fennec account. |
| [UnauthenticatedAccount](-unauthenticated-account/index.md) | `data class UnauthenticatedAccount : `[`Success`](./index.md)<br>Encountered a Fennec auth state that can't be used to automatically log-in. |
| [WillAutoRetrySignInLater](-will-auto-retry-sign-in-later/index.md) | `data class WillAutoRetrySignInLater : `[`Success`](./index.md)<br>Sign-in attempt encountered a recoverable problem, and a retry will be performed later. |

### Inheritors

| Name | Summary |
|---|---|
| [NoAccount](-no-account.md) | `object NoAccount : `[`Success`](./index.md)<br>No Fennec account found. |
| [SignedInIntoAuthenticatedAccount](-signed-in-into-authenticated-account/index.md) | `data class SignedInIntoAuthenticatedAccount : `[`Success`](./index.md)<br>Successfully signed-in with an authenticated Fennec account. |
| [UnauthenticatedAccount](-unauthenticated-account/index.md) | `data class UnauthenticatedAccount : `[`Success`](./index.md)<br>Encountered a Fennec auth state that can't be used to automatically log-in. |
| [WillAutoRetrySignInLater](-will-auto-retry-sign-in-later/index.md) | `data class WillAutoRetrySignInLater : `[`Success`](./index.md)<br>Sign-in attempt encountered a recoverable problem, and a retry will be performed later. |
