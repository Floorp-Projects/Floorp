[android-components](../../index.md) / [mozilla.components.concept.engine.selection](../index.md) / [SelectionActionDelegate](index.md) / [isActionAvailable](./is-action-available.md)

# isActionAvailable

`abstract fun isActionAvailable(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectedText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/selection/SelectionActionDelegate.kt#L25)

Checks if an action can be shown on a new selection context menu.

**Returns**
whether or not the the custom action with the id of [id](is-action-available.md#mozilla.components.concept.engine.selection.SelectionActionDelegate$isActionAvailable(kotlin.String, kotlin.String)/id) is currently available
which may be informed by [selectedText](is-action-available.md#mozilla.components.concept.engine.selection.SelectionActionDelegate$isActionAvailable(kotlin.String, kotlin.String)/selectedText).

