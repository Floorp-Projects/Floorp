[android-components](../../index.md) / [mozilla.components.feature.p2p](../index.md) / [P2PFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/P2PFeature.kt#L44)

Overrides [PermissionsFeature.onNeedToRequestPermissions](../../mozilla.components.support.base.feature/-permissions-feature/on-need-to-request-permissions.md)

A callback invoked when permissions need to be requested by the feature before
it can complete its task. Once the request is completed, [onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md)
needs to be invoked.

