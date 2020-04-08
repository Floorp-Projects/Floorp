[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.selection](../index.md) / [GeckoSelectionActionDelegate](./index.md)

# GeckoSelectionActionDelegate

`open class GeckoSelectionActionDelegate : `[`BasicSelectionActionDelegate`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/BasicSelectionActionDelegate.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/selection/GeckoSelectionActionDelegate.kt#L19)

An adapter between the GV [BasicSelectionActionDelegate](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/BasicSelectionActionDelegate.html) and a generic [SelectionActionDelegate](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md).

### Parameters

`customDelegate` - handles as much of this logic as possible.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoSelectionActionDelegate(activity: <ERROR CLASS>, customDelegate: `[`SelectionActionDelegate`](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md)`)`<br>An adapter between the GV [BasicSelectionActionDelegate](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/BasicSelectionActionDelegate.html) and a generic [SelectionActionDelegate](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getAllActions](get-all-actions.md) | `open fun getAllActions(): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [isActionAvailable](is-action-available.md) | `open fun isActionAvailable(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [performAction](perform-action.md) | `open fun performAction(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, item: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [prepareAction](prepare-action.md) | `open fun prepareAction(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, item: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [maybeCreate](maybe-create.md) | `fun maybeCreate(context: <ERROR CLASS>, customDelegate: `[`SelectionActionDelegate`](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md)`?): `[`GeckoSelectionActionDelegate`](./index.md)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
