[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [invalidateActions](./invalidate-actions.md)

# invalidateActions

`fun invalidateActions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L559)

Declare that the actions (navigation actions, browser actions, page actions) have changed and
should be updated if needed.

The toolbar will call the visible lambda of every action to determine whether a
view for this action should be added or removed. Additionally bind will be
called on every visible action to update its view.

