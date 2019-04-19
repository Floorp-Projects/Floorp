[android-components](../index.md) / [mozilla.components.browser.session.selector](index.md) / [findSessionOrSelected](./find-session-or-selected.md)

# findSessionOrSelected

`fun `[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`.findSessionOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`SessionState`](../mozilla.components.browser.session.state/-session-state/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/selector/Selectors.kt#L36)

Finds and returns the session with the given id or the selected session if no id was provided (null). Returns null
if no matching session could be found or if no selected session exists.

