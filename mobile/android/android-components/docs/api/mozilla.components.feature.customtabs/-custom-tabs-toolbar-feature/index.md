[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabsToolbarFeature](./index.md)

# CustomTabsToolbarFeature

`class CustomTabsToolbarFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabsToolbarFeature.kt#L46)

Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.

### Parameters

`toolbar` - Reference to the browser toolbar, so that the color and menu items can be set.

`sessionId` - ID of the custom tab session. No-op if null or invalid.

`menuBuilder` - Menu builder reference to pull menu options from.

`menuItemIndex` - Location to insert any custom menu options into the predefined menu list.

`window` - Reference to the window so the navigation bar color can be set.

`shareListener` - Invoked when the share button is pressed.

`closeListener` - Invoked when the close button is pressed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabsToolbarFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, toolbar: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, menuBuilder: `[`BrowserMenuBuilder`](../../mozilla.components.browser.menu/-browser-menu-builder/index.md)`? = null, menuItemIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = menuBuilder?.items?.size ?: 0, window: <ERROR CLASS>? = null, shareListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, closeListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig. |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>When the back button is pressed if not initialized returns false, when initialized removes the current Custom Tabs session and returns true. Should be called when the back button is pressed. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initializes the feature and registers the [CustomTabSessionTitleObserver](../../mozilla.components.feature.customtabs.feature/-custom-tab-session-title-observer/index.md). |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters the [CustomTabSessionTitleObserver](../../mozilla.components.feature.customtabs.feature/-custom-tab-session-title-observer/index.md). |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
