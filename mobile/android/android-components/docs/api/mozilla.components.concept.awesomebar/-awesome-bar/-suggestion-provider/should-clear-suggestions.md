[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [SuggestionProvider](index.md) / [shouldClearSuggestions](./should-clear-suggestions.md)

# shouldClearSuggestions

`open val shouldClearSuggestions: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L172)

If true an [AwesomeBar](../index.md) implementation can clear the previous suggestions of this provider as soon as the
user continues to type. If this is false an [AwesomeBar](../index.md) implementation is allowed to keep the previous
suggestions around until the provider returns a new list of suggestions for the updated text.

