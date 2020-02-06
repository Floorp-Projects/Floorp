[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FxaMigrationResult](../index.md) / [Failure](./index.md)

# Failure

`sealed class Failure : `[`FxaMigrationResult`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L92)

Failure variants of an FxA migration.

### Types

| Name | Summary |
|---|---|
| [CorruptAccountState](-corrupt-account-state/index.md) | `data class CorruptAccountState : `[`Failure`](./index.md)<br>Failed to process Fennec's auth state due to an exception [e](-corrupt-account-state/e.md). |
| [CustomServerConfigPresent](-custom-server-config-present/index.md) | `data class CustomServerConfigPresent : `[`Failure`](./index.md)<br>Encountered a Fennec with customized token/idp server endpoints. |
| [FailedToSignIntoAuthenticatedAccount](-failed-to-sign-into-authenticated-account/index.md) | `data class FailedToSignIntoAuthenticatedAccount : `[`Failure`](./index.md)<br>Failed to sign in into an authenticated account. Currently, this could be either due to network failures, invalid credentials, or server-side issues. |
| [UnsupportedVersions](-unsupported-versions/index.md) | `data class UnsupportedVersions : `[`Failure`](./index.md)<br>Encountered an unsupported version of Fennec's auth state. |

### Inheritors

| Name | Summary |
|---|---|
| [CorruptAccountState](-corrupt-account-state/index.md) | `data class CorruptAccountState : `[`Failure`](./index.md)<br>Failed to process Fennec's auth state due to an exception [e](-corrupt-account-state/e.md). |
| [CustomServerConfigPresent](-custom-server-config-present/index.md) | `data class CustomServerConfigPresent : `[`Failure`](./index.md)<br>Encountered a Fennec with customized token/idp server endpoints. |
| [FailedToSignIntoAuthenticatedAccount](-failed-to-sign-into-authenticated-account/index.md) | `data class FailedToSignIntoAuthenticatedAccount : `[`Failure`](./index.md)<br>Failed to sign in into an authenticated account. Currently, this could be either due to network failures, invalid credentials, or server-side issues. |
| [UnsupportedVersions](-unsupported-versions/index.md) | `data class UnsupportedVersions : `[`Failure`](./index.md)<br>Encountered an unsupported version of Fennec's auth state. |
