[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionSpace](./index.md)

# ActionSpace

`open class ActionSpace : `[`Action`](../-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L317)

An "empty" action with a desired width to be used as "placeholder".

### Parameters

`desiredWidth` - The desired width in density independent pixels for this action.

`padding` - A optional custom padding.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ActionSpace(desiredWidth: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null)`<br>An "empty" action with a desired width to be used as "placeholder". |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../-action/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |
