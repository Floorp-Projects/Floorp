[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`var onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsFeature.kt#L56)

Overrides [PermissionsFeature.onNeedToRequestPermissions](../../mozilla.components.support.base.feature/-permissions-feature/on-need-to-request-permissions.md)

a callback invoked when permissions
need to be requested before a download can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.

### Property

`onNeedToRequestPermissions` - a callback invoked when permissions
need to be requested before a download can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.