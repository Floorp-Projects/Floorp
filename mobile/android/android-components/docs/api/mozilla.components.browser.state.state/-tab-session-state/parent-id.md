[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TabSessionState](index.md) / [parentId](./parent-id.md)

# parentId

`val parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TabSessionState.kt#L30)

the parent ID of this tab or null if this tab has no
parent. The parent tab is usually the tab that initiated opening this
tab (e.g. the user clicked a link with target="_blank" or selected
"open in new tab" or a "window.open" was triggered).

### Property

`parentId` - the parent ID of this tab or null if this tab has no
parent. The parent tab is usually the tab that initiated opening this
tab (e.g. the user clicked a link with target="_blank" or selected
"open in new tab" or a "window.open" was triggered).