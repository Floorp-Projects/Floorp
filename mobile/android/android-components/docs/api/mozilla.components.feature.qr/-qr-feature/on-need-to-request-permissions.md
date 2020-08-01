[android-components](../../index.md) / [mozilla.components.feature.qr](../index.md) / [QrFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/QrFeature.kt#L40)

Overrides [PermissionsFeature.onNeedToRequestPermissions](../../mozilla.components.support.base.feature/-permissions-feature/on-need-to-request-permissions.md)

a callback invoked when permissions
need to be requested before a QR scan can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. This feature
will request [android.Manifest.permission.CAMERA](#).

### Property

`onNeedToRequestPermissions` - a callback invoked when permissions
need to be requested before a QR scan can be performed. Once the request
is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. This feature
will request [android.Manifest.permission.CAMERA](#).