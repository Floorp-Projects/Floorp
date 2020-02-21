[android-components](../index.md) / [mozilla.components.browser.state.selector](index.md) / [findTabOrCustomTabOrSelectedTab](./find-tab-or-custom-tab-or-selected-tab.md)

# findTabOrCustomTabOrSelectedTab

`fun `[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`.findTabOrCustomTabOrSelectedTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`SessionState`](../mozilla.components.browser.state.state/-session-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/selector/Selectors.kt#L73)

Finds and returns the tab with the given id or the selected tab if no id was provided (null). Returns null
if no matching tab could be found or if no selected tab exists.

### Parameters

`tabId` - An optional ID of a tab. If not provided or null then the selected tab will be returned.

**Return**
The custom tab with the provided ID or the selected tav if no id was provided.

