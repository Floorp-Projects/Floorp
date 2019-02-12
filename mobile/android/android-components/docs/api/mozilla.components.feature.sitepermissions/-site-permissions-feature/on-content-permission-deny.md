[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsFeature](index.md) / [onContentPermissionDeny](./on-content-permission-deny.md)

# onContentPermissionDeny

`fun onContentPermissionDeny(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsFeature.kt#L92)

Notifies that the permissions requested by this [sessionId](on-content-permission-deny.md#mozilla.components.feature.sitepermissions.SitePermissionsFeature$onContentPermissionDeny(kotlin.String, kotlin.String)/sessionId) were rejected.

### Parameters

`sessionId` - this is the id of the session which requested the permissions.

`url` - the url which requested the permissions.