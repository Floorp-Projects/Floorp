[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [SuggestionLayout](index.md) / [createViewHolder](./create-view-holder.md)

# createViewHolder

`abstract fun createViewHolder(awesomeBar: `[`BrowserAwesomeBar`](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md)`, view: <ERROR CLASS>, @LayoutRes layoutId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`SuggestionViewHolder`](../-suggestion-view-holder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/SuggestionLayout.kt#L29)

Creates and returns a [SuggestionViewHolder](../-suggestion-view-holder/index.md) instance for the provided [View](#). The [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) will call
[SuggestionViewHolder.bind](../-suggestion-view-holder/bind.md) once this view holder should display the data of a specific [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md).

