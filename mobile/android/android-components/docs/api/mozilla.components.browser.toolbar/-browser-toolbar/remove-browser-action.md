[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [removeBrowserAction](./remove-browser-action.md)

# removeBrowserAction

`fun removeBrowserAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L245)

Overrides [Toolbar.removeBrowserAction](../../mozilla.components.concept.toolbar/-toolbar/remove-browser-action.md)

Removes a previously added browser action (see [addBrowserAction](add-browser-action.md)). If the provided
action was never added, this method has no effect.

### Parameters

`action` - the action to remove.