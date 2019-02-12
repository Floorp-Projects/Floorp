[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsFeature](index.md) / [onContentPermissionGranted](./on-content-permission-granted.md)

# onContentPermissionGranted

`fun onContentPermissionGranted(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsFeature.kt#L76)

Notifies that the list of [permissions](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/permissions) have been granted for the [sessionId](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/sessionId) and [url](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/url).

### Parameters

`sessionId` - this is the id of the session which requested the permissions.

`url` - the url which requested the permissions.

`permissions` - the list of [permissions](on-content-permission-granted.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionGranted(kotlin.String, kotlin.String, kotlin.collections.List((mozilla.components.concept.engine.permission.Permission)))/permissions) that have been granted.