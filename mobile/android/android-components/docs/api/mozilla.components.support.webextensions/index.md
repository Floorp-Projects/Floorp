[android-components](../index.md) / [mozilla.components.support.webextensions](./index.md)

## Package mozilla.components.support.webextensions

### Types

| Name | Summary |
|---|---|
| [WebExtensionController](-web-extension-controller/index.md) | `class WebExtensionController`<br>Provides functionality to feature modules that need to interact with a web extension. |
| [WebExtensionPopupFeature](-web-extension-popup-feature/index.md) | `class WebExtensionPopupFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation that opens popups for web extensions browser actions. |
| [WebExtensionSupport](-web-extension-support/index.md) | `object WebExtensionSupport`<br>Provides functionality to make sure web extension related events in the [WebExtensionRuntime](../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) are reflected in the browser state by dispatching the corresponding actions to the [BrowserStore](../mozilla.components.browser.state.store/-browser-store/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [onUpdatePermissionRequest](on-update-permission-request.md) | `typealias onUpdatePermissionRequest = (current: `[`WebExtension`](../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Function to relay the permission request to the app / user. |
