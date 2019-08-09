[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [addBrowserAction](./add-browser-action.md)

# addBrowserAction

`fun addBrowserAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L585)

Overrides [Toolbar.addBrowserAction](../../mozilla.components.concept.toolbar/-toolbar/add-browser-action.md)

Adds an action to be displayed on the right side of the toolbar (outside of the URL bounding
box) in display mode.

If there is not enough room to show all icons then some icons may be moved to an overflow
menu.

Related:
https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action

