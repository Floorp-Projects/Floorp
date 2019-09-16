[android-components](../../../index.md) / [mozilla.components.feature.contextmenu](../../index.md) / [ContextMenuCandidate](../index.md) / [SnackbarDelegate](index.md) / [show](./show.md)

# show

`abstract fun show(snackBarParentView: <ERROR CLASS>, text: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, duration: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, action: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, listener: (<ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuCandidate.kt#L246)

Displays a snackbar.

### Parameters

`snackBarParentView` - The view to find a parent from for displaying the Snackbar.

`text` - The text to show. Can be formatted text.

`duration` - How long to display the message

`action` - String resource to display for the action.

`listener` - callback to be invoked when the action is clicked