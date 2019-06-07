[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [SuggestionProvider](index.md) / [onInputStarted](./on-input-started.md)

# onInputStarted

`open fun onInputStarted(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../-suggestion/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L145)

Fired when the user starts interacting with the awesome bar by entering text in the toolbar.

The provider has the option to return an initial list of suggestions that will be displayed before the
user has entered/modified any of the text.

