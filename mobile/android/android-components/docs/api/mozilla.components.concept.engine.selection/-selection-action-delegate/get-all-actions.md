[android-components](../../index.md) / [mozilla.components.concept.engine.selection](../index.md) / [SelectionActionDelegate](index.md) / [getAllActions](./get-all-actions.md)

# getAllActions

`abstract fun getAllActions(): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/selection/SelectionActionDelegate.kt#L17)

Gets Strings representing all possible selection actions.

**Returns**
String IDs for each action that could possibly be shown in the context menu. This
array must include all actions, available or not, and must not change over the class lifetime.

