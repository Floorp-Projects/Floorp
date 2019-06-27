[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SitePermissionsFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, storage: `[`SitePermissionsStorage`](../-site-permissions-storage/index.md)` = SitePermissionsStorage(context), sitePermissionsRules: `[`SitePermissionsRules`](../-site-permissions-rules/index.md)`? = null, fragmentManager: FragmentManager, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = null, dialogConfig: `[`DialogConfig`](-dialog-config/index.md)`? = null, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`)`

This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display
a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or
[Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events.
Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed.

