[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchSuggestionProvider](index.md) / [shouldClearSuggestions](./should-clear-suggestions.md)

# shouldClearSuggestions

`val shouldClearSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/provider/SearchSuggestionProvider.kt#L190)

Overrides [SuggestionProvider.shouldClearSuggestions](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/should-clear-suggestions.md)

If true an [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation can clear the previous suggestions of this provider as soon as the
user continues to type. If this is false an [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation is allowed to keep the previous
suggestions around until the provider returns a new list of suggestions for the updated text.

