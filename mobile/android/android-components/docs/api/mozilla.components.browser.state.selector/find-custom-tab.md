[android-components](../index.md) / [mozilla.components.browser.state.selector](index.md) / [findCustomTab](./find-custom-tab.md)

# findCustomTab

`fun `[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`.findCustomTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`CustomTabSessionState`](../mozilla.components.browser.state.state/-custom-tab-session-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/selector/Selectors.kt#L40)

Finds and returns the Custom Tab with the given id. Returns null if no matching tab could be
found.

### Parameters

`tabId` - The ID of the custom tab to search for.

**Return**
The [CustomTabSessionState](../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) with the provided [tabId](find-custom-tab.md#mozilla.components.browser.state.selector$findCustomTab(mozilla.components.browser.state.state.BrowserState, kotlin.String)/tabId) or null if it could not be found.

