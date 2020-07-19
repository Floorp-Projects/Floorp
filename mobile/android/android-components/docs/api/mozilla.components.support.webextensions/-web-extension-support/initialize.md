[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionSupport](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, openPopupInTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onNewTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = null, onCloseTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onSelectTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onUpdatePermissionRequest: `[`onUpdatePermissionRequest`](../on-update-permission-request.md)`? = { _, _, _, _ -> }, onExtensionsLoaded: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionSupport.kt#L170)

Registers a listener for web extension related events on the provided
[WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) and reacts by dispatching the corresponding actions to the
provided [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Parameters

`runtime` - the browser [WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) to use.

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

`openPopupInTab` - (optional) flag to determine whether a browser or page action would
display a web extension popup in a tab or not. Defaults to false.

`onNewTabOverride` - (optional) override of behaviour that should
be triggered when web extensions open a new tab e.g. when dispatching
to the store isn't sufficient while migrating from browser-session
to browser-state. This is a lambda accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), the
[EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to use, as well as the URL to load, return the ID of
the created session.

`onCloseTabOverride` - (optional) override of behaviour that should
be triggered when web extensions close tabs e.g. when dispatching
to the store isn't sufficient while migrating from browser-session
to browser-state. This is a lambda accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) and
the session/tab ID to close.

`onSelectTabOverride` - (optional) override of behaviour that should
be triggered when a tab is selected to display a web extension popup.
This is a lambda accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) and the session/tab ID to
select.

`onUpdatePermissionRequest` - (optional) Invoked when a web extension has changed its
permissions while trying to update to a new version. This requires user interaction as
the updated extension will not be installed, until the user grants the new permissions.

`onExtensionsLoaded` - (optional) callback invoked when the extensions are loaded by the
engine. Note that the UI (browser/page actions etc.) may not be initialized at this point.
System add-ons (built-in extensions) will not be passed along.