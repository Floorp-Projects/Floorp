[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [SuggestionViewHolder](index.md) / [bind](./bind.md)

# bind

`abstract fun bind(suggestion: `[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`, selectionListener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/SuggestionViewHolder.kt#L23)

Binds the views in the holder to the [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md).

Contract: When a suggestion was selected/clicked the view will invoke the appropriate callback of the suggestion
([AwesomeBar.Suggestion.onSuggestionClicked](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/on-suggestion-clicked.md) or [AwesomeBar.Suggestion.onChipClicked](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/on-chip-clicked.md)) as well as the provided
[selectionListener](bind.md#mozilla.components.browser.awesomebar.layout.SuggestionViewHolder$bind(mozilla.components.concept.awesomebar.AwesomeBar.Suggestion, kotlin.Function0((kotlin.Unit)))/selectionListener) function.

