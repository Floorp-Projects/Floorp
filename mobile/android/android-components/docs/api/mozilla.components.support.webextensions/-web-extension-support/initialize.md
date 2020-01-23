[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionSupport](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, onNewTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = null, onCloseTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onSelectTabOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onActionPopupToggleOverride: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onUpdatePermissionRequest: (current: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, updated: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, newPermissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, onPermissionsGranted: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _, _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionSupport.kt#L102)

Registers a listener for web extension related events on the provided
[Engine](../../mozilla.components.concept.engine/-engine/index.md) and reacts by dispatching the corresponding actions to the
provided [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Parameters

`engine` - the browser [Engine](../../mozilla.components.concept.engine/-engine/index.md) to use.

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

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

`onActionPopupToggleOverride` - (optional) override of behaviour that should
be triggered when a browser or page action is toggled to display a web extension popup.
This is a lambda accepting the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md).

`onUpdatePermissionRequest` - (optional) Invoked when a web extension has changed its
permissions while trying to update to a new version. This requires user interaction as
the updated extension will not be installed, until the user grants the new permissions.