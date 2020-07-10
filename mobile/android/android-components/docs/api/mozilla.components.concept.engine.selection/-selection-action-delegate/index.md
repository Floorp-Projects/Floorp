[android-components](../../index.md) / [mozilla.components.concept.engine.selection](../index.md) / [SelectionActionDelegate](./index.md)

# SelectionActionDelegate

`interface SelectionActionDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/selection/SelectionActionDelegate.kt#L10)

Generic delegate for handling the context menu that is shown when text is selected.

### Functions

| Name | Summary |
|---|---|
| [getActionTitle](get-action-title.md) | `abstract fun getActionTitle(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?`<br>Gets a title to be shown in the selection context menu. |
| [getAllActions](get-all-actions.md) | `abstract fun getAllActions(): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Gets Strings representing all possible selection actions. |
| [isActionAvailable](is-action-available.md) | `abstract fun isActionAvailable(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectedText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if an action can be shown on a new selection context menu. |
| [performAction](perform-action.md) | `abstract fun performAction(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectedText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Should perform the action with the id of [id](perform-action.md#mozilla.components.concept.engine.selection.SelectionActionDelegate$performAction(kotlin.String, kotlin.String)/id). |
| [sortedActions](sorted-actions.md) | `abstract fun sortedActions(actions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Takes in a list of actions and sorts them. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultSelectionActionDelegate](../../mozilla.components.feature.contextmenu/-default-selection-action-delegate/index.md) | `class DefaultSelectionActionDelegate : `[`SelectionActionDelegate`](./index.md)<br>Adds normal and private search buttons to text selection context menus. Also adds share, email, and call actions which are optionally displayed. |
