[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [ClipboardSuggestionProvider](index.md) / [onInputStarted](./on-input-started.md)

# onInputStarted

`fun onInputStarted(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/provider/ClipboardSuggestionProvider.kt#L47)

Overrides [SuggestionProvider.onInputStarted](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/on-input-started.md)

Fired when the user starts interacting with the awesome bar by entering text in the toolbar.

The provider has the option to return an initial list of suggestions that will be displayed before the
user has entered/modified any of the text.

