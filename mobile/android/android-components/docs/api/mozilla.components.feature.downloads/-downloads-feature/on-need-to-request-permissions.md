[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`var onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsFeature.kt#L48)

a callback invoked when permissions
need to be requested before a download can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.

### Property

`onNeedToRequestPermissions` - a callback invoked when permissions
need to be requested before a download can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.