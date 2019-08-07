[android-components](../../../index.md) / [mozilla.components.concept.toolbar](../../index.md) / [Toolbar](../index.md) / [ActionImage](./index.md)

# ActionImage

`open class ActionImage : `[`Action`](../-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L338)

An action that just shows a static, non-clickable image.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - Optional content description to be used. If no content description
    is provided then this view will be treated as not important for
    accessibility.

`padding` - A optional custom padding.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ActionImage(imageDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)`? = null)`<br>An action that just shows a static, non-clickable image. |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../-action/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |
