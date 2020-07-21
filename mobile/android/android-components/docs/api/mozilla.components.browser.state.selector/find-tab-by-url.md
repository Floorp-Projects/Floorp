[android-components](../index.md) / [mozilla.components.browser.state.selector](index.md) / [findTabByUrl](./find-tab-by-url.md)

# findTabByUrl

`fun `[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`.findTabByUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TabSessionState`](../mozilla.components.browser.state.state/-tab-session-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/selector/Selectors.kt#L87)

Finds and returns the tab with the given url. Returns null if no matching tab could be found.

### Parameters

`url` - A mandatory url of the searched tab.

**Return**
The tab with the provided url

