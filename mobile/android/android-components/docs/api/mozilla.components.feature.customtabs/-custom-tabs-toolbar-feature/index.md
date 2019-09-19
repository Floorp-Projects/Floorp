[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabsToolbarFeature](./index.md)

# CustomTabsToolbarFeature

`class CustomTabsToolbarFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../../mozilla.components.support.base.feature/-back-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabsToolbarFeature.kt#L36)

Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabsToolbarFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, toolbar: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, menuBuilder: `[`BrowserMenuBuilder`](../../mozilla.components.browser.menu/-browser-menu-builder/index.md)`? = null, menuItemIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = menuBuilder?.items?.size ?: 0, window: <ERROR CLASS>? = null, shareListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, closeListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig. |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>When the back button is pressed if not initialized returns false, when initialized removes the current Custom Tabs session and returns true. Should be called when the back button is pressed. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [TITLE_TEXT_SIZE](-t-i-t-l-e_-t-e-x-t_-s-i-z-e.md) | `const val TITLE_TEXT_SIZE: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html) |
| [URL_TEXT_SIZE](-u-r-l_-t-e-x-t_-s-i-z-e.md) | `const val URL_TEXT_SIZE: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html) |
