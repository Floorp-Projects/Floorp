[android-components](../index.md) / [mozilla.components.browser.state.selector](index.md) / [findTab](./find-tab.md)

# findTab

`fun `[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`.findTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TabSessionState`](../mozilla.components.browser.state.state/-tab-session-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/selector/Selectors.kt#L29)

Finds and returns the tab with the given id. Returns null if no matching tab could be
found.

### Parameters

`tabId` - The ID of the tab to search for.

**Return**
The [TabSessionState](../mozilla.components.browser.state.state/-tab-session-state/index.md) with the provided [tabId](find-tab.md#mozilla.components.browser.state.selector$findTab(mozilla.components.browser.state.state.BrowserState, kotlin.String)/tabId) or null if it could not be found.

