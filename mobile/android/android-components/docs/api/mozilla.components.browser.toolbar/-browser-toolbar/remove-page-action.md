[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [removePageAction](./remove-page-action.md)

# removePageAction

`fun removePageAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L255)

Overrides [Toolbar.removePageAction](../../mozilla.components.concept.toolbar/-toolbar/remove-page-action.md)

Removes a previously added page action (see [addPageAction](add-page-action.md)). If the provided
action was never added, this method has no effect.

### Parameters

`action` - the action to remove.