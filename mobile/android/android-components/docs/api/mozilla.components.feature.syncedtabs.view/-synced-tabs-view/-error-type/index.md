[android-components](../../../index.md) / [mozilla.components.feature.syncedtabs.view](../../index.md) / [SyncedTabsView](../index.md) / [ErrorType](./index.md)

# ErrorType

`enum class ErrorType` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/view/SyncedTabsView.kt#L61)

The various types of errors that can occur from syncing tabs.

### Enum Values

| Name | Summary |
|---|---|
| [NO_TABS_AVAILABLE](-n-o_-t-a-b-s_-a-v-a-i-l-a-b-l-e.md) | Other devices found but there are no tabs to sync. |
| [MULTIPLE_DEVICES_UNAVAILABLE](-m-u-l-t-i-p-l-e_-d-e-v-i-c-e-s_-u-n-a-v-a-i-l-a-b-l-e.md) | There are no other devices found with this account and therefore no tabs to sync. |
| [SYNC_ENGINE_UNAVAILABLE](-s-y-n-c_-e-n-g-i-n-e_-u-n-a-v-a-i-l-a-b-l-e.md) | The engine for syncing tabs is unavailable. This is mostly due to a user turning off the feature on the Firefox Sync account. |
| [SYNC_UNAVAILABLE](-s-y-n-c_-u-n-a-v-a-i-l-a-b-l-e.md) | There is no Firefox Sync account available. A user needs to sign-in before this feature. |
| [SYNC_NEEDS_REAUTHENTICATION](-s-y-n-c_-n-e-e-d-s_-r-e-a-u-t-h-e-n-t-i-c-a-t-i-o-n.md) | The Firefox Sync account requires user-intervention to re-authenticate the account. |
