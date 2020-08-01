[android-components](../index.md) / [mozilla.components.support.webextensions](index.md) / [onUpdatePermissionRequest](./on-update-permission-request.md)

# onUpdatePermissionRequest

`typealias onUpdatePermissionRequest = (current: `[`WebExtension`](../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionSupport.kt#L40)

Function to relay the permission request to the app / user.

