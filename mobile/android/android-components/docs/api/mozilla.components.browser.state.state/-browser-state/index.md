[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [BrowserState](./index.md)

# BrowserState

`data class BrowserState : `[`State`](../../mozilla.components.lib.state/-state.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/BrowserState.kt#L16)

Value type that represents the complete state of the browser/engine.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserState(tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../-tab-session-state/index.md)`> = emptyList(), selectedTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, customTabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabSessionState`](../-custom-tab-session-state/index.md)`> = emptyList())`<br>Value type that represents the complete state of the browser/engine. |

### Properties

| Name | Summary |
|---|---|
| [customTabs](custom-tabs.md) | `val customTabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabSessionState`](../-custom-tab-session-state/index.md)`>`<br>the list of custom tabs, defaults to an empty list. |
| [selectedTabId](selected-tab-id.md) | `val selectedTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the ID of the currently selected (active) tab. |
| [tabs](tabs.md) | `val tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../-tab-session-state/index.md)`>`<br>the list of open tabs, defaults to an empty list. |

### Extension Properties

| Name | Summary |
|---|---|
| [normalTabs](../../mozilla.components.browser.state.selector/normal-tabs.md) | `val `[`BrowserState`](./index.md)`.normalTabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../-tab-session-state/index.md)`>`<br>List of normal (non-private) tabs. |
| [privateTabs](../../mozilla.components.browser.state.selector/private-tabs.md) | `val `[`BrowserState`](./index.md)`.privateTabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../-tab-session-state/index.md)`>`<br>List of private tabs. |
| [selectedTab](../../mozilla.components.browser.state.selector/selected-tab.md) | `val `[`BrowserState`](./index.md)`.selectedTab: `[`TabSessionState`](../-tab-session-state/index.md)`?`<br>The currently selected tab if there's one. |

### Extension Functions

| Name | Summary |
|---|---|
| [findCustomTab](../../mozilla.components.browser.state.selector/find-custom-tab.md) | `fun `[`BrowserState`](./index.md)`.findCustomTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`CustomTabSessionState`](../-custom-tab-session-state/index.md)`?`<br>Finds and returns the Custom Tab with the given id. Returns null if no matching tab could be found. |
| [findCustomTabOrSelectedTab](../../mozilla.components.browser.state.selector/find-custom-tab-or-selected-tab.md) | `fun `[`BrowserState`](./index.md)`.findCustomTabOrSelectedTab(customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`SessionState`](../-session-state/index.md)`?`<br>Finds and returns the tab with the given id or the selected tab if no id was provided (null). Returns null if no matching tab could be found or if no selected tab exists. |
| [findTab](../../mozilla.components.browser.state.selector/find-tab.md) | `fun `[`BrowserState`](./index.md)`.findTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TabSessionState`](../-tab-session-state/index.md)`?`<br>Finds and returns the tab with the given id. Returns null if no matching tab could be found. |
| [findTabOrCustomTab](../../mozilla.components.browser.state.selector/find-tab-or-custom-tab.md) | `fun `[`BrowserState`](./index.md)`.findTabOrCustomTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SessionState`](../-session-state/index.md)`?`<br>Finds and returns the [TabSessionState](../-tab-session-state/index.md) or [CustomTabSessionState](../-custom-tab-session-state/index.md) with the given [tabId](../../mozilla.components.browser.state.selector/find-tab-or-custom-tab.md#mozilla.components.browser.state.selector$findTabOrCustomTab(mozilla.components.browser.state.state.BrowserState, kotlin.String)/tabId). |
