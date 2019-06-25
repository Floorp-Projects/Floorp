[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsFeature](./index.md)

# SitePermissionsFeature

`class SitePermissionsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsFeature.kt#L64)

This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display
a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or
[Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events.
Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed.

### Types

| Name | Summary |
|---|---|
| [DialogConfig](-dialog-config/index.md) | `data class DialogConfig`<br>Customization options for feature request dialog |
| [PromptsStyling](-prompts-styling/index.md) | `data class PromptsStyling` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissionsFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, storage: `[`SitePermissionsStorage`](../-site-permissions-storage/index.md)` = SitePermissionsStorage(context), sitePermissionsRules: `[`SitePermissionsRules`](../-site-permissions-rules/index.md)`? = null, fragmentManager: FragmentManager, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = null, dialogConfig: `[`DialogConfig`](-dialog-config/index.md)`? = null, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)`)`<br>This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or [Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events. Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed. |

### Properties

| Name | Summary |
|---|---|
| [promptsStyling](prompts-styling.md) | `var promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`?`<br>optional styling for prompts. |
| [sitePermissionsRules](site-permissions-rules.md) | `var sitePermissionsRules: `[`SitePermissionsRules`](../-site-permissions-rules/index.md)`?`<br>indicates how permissions should behave per permission category. |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions requested were completed. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
