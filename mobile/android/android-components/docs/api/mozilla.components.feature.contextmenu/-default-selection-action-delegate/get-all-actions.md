[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [DefaultSelectionActionDelegate](index.md) / [getAllActions](./get-all-actions.md)

# getAllActions

`fun getAllActions(): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/DefaultSelectionActionDelegate.kt#L48)

Overrides [SelectionActionDelegate.getAllActions](../../mozilla.components.concept.engine.selection/-selection-action-delegate/get-all-actions.md)

Gets Strings representing all possible selection actions.

**Returns**
String IDs for each action that could possibly be shown in the context menu. This
array must include all actions, available or not, and must not change over the class lifetime.

