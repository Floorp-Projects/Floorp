[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onInstallPermissionRequest](./on-install-permission-request.md)

# onInstallPermissionRequest

`open fun onInstallPermissionRequest(extension: `[`WebExtension`](../-web-extension/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L108)

Invoked during installation of a [WebExtension](../-web-extension/index.md) to confirm the required permissions.

### Parameters

`extension` - the extension being installed. The required permissions can be
accessed using [WebExtension.getMetadata](../-web-extension/get-metadata.md) and [Metadata.permissions](../-metadata/permissions.md).

**Return**
whether or not installation should process i.e. the permissions have been
granted.

