[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsFeature](./index.md)

# SitePermissionsFeature

`class SitePermissionsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsFeature.kt#L30)

This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display
a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or
[Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events.
Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissionsFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)`)`<br>This feature will subscribe to the currently selected [Session](../../mozilla.components.browser.session/-session/index.md) and display a suitable dialogs based on [Session.Observer.onAppPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-app-permission-requested.md) or [Session.Observer.onContentPermissionRequested](../../mozilla.components.browser.session/-session/-observer/on-content-permission-requested.md)  events. Once the dialog is closed the [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md) will be consumed. |

### Functions

| Name | Summary |
|---|---|
| [onContentPermissionDeny](on-content-permission-deny.md) | `fun onContentPermissionDeny(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies that the permissions requested by this [sessionId](on-content-permission-deny.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionDeny(kotlin.String, kotlin.String)/sessionId) were rejected. |
| [onContentPermissionGranted](on-content-permission-granted.md) | `fun onContentPermissionGranted(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies that the list of [permissions](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/permissions) have been granted for the [sessionId](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/sessionId) and [url](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/url). |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions requested were completed. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
