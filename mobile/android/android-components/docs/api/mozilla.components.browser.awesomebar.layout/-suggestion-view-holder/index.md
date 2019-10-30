[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [SuggestionViewHolder](./index.md)

# SuggestionViewHolder

`abstract class SuggestionViewHolder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/SuggestionViewHolder.kt#L13)

A view holder implementation for displaying an [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SuggestionViewHolder(view: <ERROR CLASS>)`<br>A view holder implementation for displaying an [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md). |

### Properties

| Name | Summary |
|---|---|
| [view](view.md) | `val view: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `abstract fun bind(suggestion: `[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`, selectionListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Binds the views in the holder to the [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md). |
| [recycle](recycle.md) | `open fun recycle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies this [SuggestionViewHolder](./index.md) that it has been recycled. If this holder (or its views) keep references to large or expensive data such as large bitmaps, this may be a good place to release those resources. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
