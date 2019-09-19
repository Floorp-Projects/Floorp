[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [PromptFeature](index.md) / [onNeedToRequestPermissions](./on-need-to-request-permissions.md)

# onNeedToRequestPermissions

`val onNeedToRequestPermissions: <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/PromptFeature.kt#L120)

Overrides [PermissionsFeature.onNeedToRequestPermissions](../../mozilla.components.support.base.feature/-permissions-feature/on-need-to-request-permissions.md)

a callback invoked when permissions
need to be requested before a prompt (e.g. a file picker) can be displayed.
Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.

### Property

`onNeedToRequestPermissions` - a callback invoked when permissions
need to be requested before a prompt (e.g. a file picker) can be displayed.
Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked.