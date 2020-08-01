[android-components](../index.md) / [mozilla.components.feature.sitepermissions](./index.md)

## Package mozilla.components.feature.sitepermissions

### Types

| Name | Summary |
|---|---|
| [SitePermissions](-site-permissions/index.md) | `data class SitePermissions`<br>A site permissions and its state. |
| [SitePermissionsFeature](-site-permissions-feature/index.md) | `class SitePermissionsFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../mozilla.components.support.base.feature/-permissions-feature/index.md)<br>This feature will subscribe to the currently selected [Session](../mozilla.components.browser.session/-session/index.md) and display a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or [Session.Observer.onContentPermissionRequested](../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events. Once the dialog is closed the [PermissionRequest](../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed. |
| [SitePermissionsRules](-site-permissions-rules/index.md) | `data class SitePermissionsRules`<br>Indicate how site permissions must behave by permission category. |
| [SitePermissionsStorage](-site-permissions-storage/index.md) | `class SitePermissionsStorage`<br>A storage implementation to save [SitePermissions](-site-permissions/index.md). |
