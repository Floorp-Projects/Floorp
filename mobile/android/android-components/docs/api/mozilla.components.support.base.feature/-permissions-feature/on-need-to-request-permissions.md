[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [PermissionsFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`abstract val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/PermissionsFeature.kt#L41)

A callback invoked when permissions need to be requested by the feature before
it can complete its task. Once the request is completed, [onPermissionsResult](on-permissions-result.md)
needs to be invoked.

