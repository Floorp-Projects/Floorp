[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [DefaultSelectionActionDelegate](index.md) / [isActionAvailable](./is-action-available.md)

# isActionAvailable

`fun isActionAvailable(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectedText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/DefaultSelectionActionDelegate.kt#L51)

Overrides [SelectionActionDelegate.isActionAvailable](../../mozilla.components.concept.engine.selection/-selection-action-delegate/is-action-available.md)

Checks if an action can be shown on a new selection context menu.

**Returns**
whether or not the the custom action with the id of [id](../../mozilla.components.concept.engine.selection/-selection-action-delegate/is-action-available.md#mozilla.components.concept.engine.selection.SelectionActionDelegate$isActionAvailable(kotlin.String, kotlin.String)/id) is currently available
which may be informed by [selectedText](../../mozilla.components.concept.engine.selection/-selection-action-delegate/is-action-available.md#mozilla.components.concept.engine.selection.SelectionActionDelegate$isActionAvailable(kotlin.String, kotlin.String)/selectedText).

