[android-components](../index.md) / [mozilla.components.service.fxa.sharing](./index.md)

## Package mozilla.components.service.fxa.sharing

### Types

| Name | Summary |
|---|---|
| [AccountSharing](-account-sharing/index.md) | `object AccountSharing`<br>Allows querying trusted FxA Auth provider packages on the device for instances of [ShareableAccount](-shareable-account/index.md). Once an instance of [ShareableAccount](-shareable-account/index.md) is obtained, it may be used with [FxaAccountManager.migrateAccountAsync](#) directly, or with [FirefoxAccount.migrateFromSessionTokenAsync](#) via [ShareableAccount.authInfo](-shareable-account/auth-info.md). |
| [ShareableAccount](-shareable-account/index.md) | `data class ShareableAccount`<br>Data structure describing an FxA account within another package that may be used to sign-in. |
| [ShareableAuthInfo](-shareable-auth-info/index.md) | `data class ShareableAuthInfo`<br>Data structure describing FxA and Sync credentials necessary to share an FxA account. |
