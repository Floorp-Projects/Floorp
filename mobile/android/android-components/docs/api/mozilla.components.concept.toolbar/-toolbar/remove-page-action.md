[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [Toolbar](index.md) / [removePageAction](./remove-page-action.md)

# removePageAction

`abstract fun removePageAction(action: `[`Action`](-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L119)

Removes a previously added page action (see [addBrowserAction](add-browser-action.md)). If the the provided
actions was never added, this method has no effect.

### Parameters

`action` - the action to remove.