[android-components](../index.md) / [mozilla.components.browser.state.selector](index.md) / [getNormalOrPrivateTabs](./get-normal-or-private-tabs.md)

# getNormalOrPrivateTabs

`fun `[`BrowserState`](../mozilla.components.browser.state.state/-browser-state/index.md)`.getNormalOrPrivateTabs(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../mozilla.components.browser.state.state/-tab-session-state/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/selector/Selectors.kt#L96)

Gets a list of normal or private tabs depending on the requested type.

### Parameters

`private` - If true, all private tabs will be returned.
If false, all normal tabs will be returned.