[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionSupport](./index.md)

# WebExtensionSupport

`object WebExtensionSupport` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionSupport.kt#L57)

Provides functionality to make sure web extension related events in the
[WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) are reflected in the browser state by dispatching the
corresponding actions to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

Note that this class can be removed once the browser-state migration
is completed and the [WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) (engine) has direct access to the
[BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md): https://github.com/orgs/mozilla-mobile/projects/31

### Properties

| Name | Summary |
|---|---|
| [installedExtensions](installed-extensions.md) | `val installedExtensions: `[`ConcurrentHashMap`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ConcurrentHashMap.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [awaitInitialization](await-initialization.md) | `suspend fun awaitInitialization(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Awaits for completion of the initialization process (completes when the state of all installed extensions is known). |
| [initialize](initialize.md) | `fun initialize(runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, openPopupInTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onNewTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = null, onCloseTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onSelectTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onUpdatePermissionRequest: `[`onUpdatePermissionRequest`](../on-update-permission-request.md)`? = { _, _, _, _ -> }, onExtensionsLoaded: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers a listener for web extension related events on the provided [WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) and reacts by dispatching the corresponding actions to the provided [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md). |
| [markExtensionAsUpdated](mark-extension-as-updated.md) | `fun markExtensionAsUpdated(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, updatedExtension: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the provided [updatedExtension](mark-extension-as-updated.md#mozilla.components.support.webextensions.WebExtensionSupport$markExtensionAsUpdated(mozilla.components.browser.state.store.BrowserStore, mozilla.components.concept.engine.webextension.WebExtension)/updatedExtension) as updated in the [store](mark-extension-as-updated.md#mozilla.components.support.webextensions.WebExtensionSupport$markExtensionAsUpdated(mozilla.components.browser.state.store.BrowserStore, mozilla.components.concept.engine.webextension.WebExtension)/store). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
