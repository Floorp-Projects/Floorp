[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [DefaultSuggestionLayout](./index.md)

# DefaultSuggestionLayout

`class DefaultSuggestionLayout : `[`SuggestionLayout`](../-suggestion-layout/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/DefaultSuggestionLayout.kt#L15)

Default implementation of [SuggestionLayout](../-suggestion-layout/index.md) to be used by [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultSuggestionLayout()`<br>Default implementation of [SuggestionLayout](../-suggestion-layout/index.md) to be used by [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md). |

### Functions

| Name | Summary |
|---|---|
| [createViewHolder](create-view-holder.md) | `fun createViewHolder(awesomeBar: `[`BrowserAwesomeBar`](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md)`, view: <ERROR CLASS>, layoutId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`SuggestionViewHolder`](../-suggestion-view-holder/index.md)<br>Creates and returns a [SuggestionViewHolder](../-suggestion-view-holder/index.md) instance for the provided [View](#). The [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) will call [SuggestionViewHolder.bind](../-suggestion-view-holder/bind.md) once this view holder should display the data of a specific [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md). |
| [getLayoutResource](get-layout-resource.md) | `fun getLayoutResource(suggestion: `[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns a layout resource ID to be used for this suggestion. The [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) implementation will take care of inflating the layout or re-using instances as needed. |
