[android-components](../../index.md) / [mozilla.components.feature.awesomebar.provider](../index.md) / [SearchSuggestionProvider](index.md) / [onInputChanged](./on-input-changed.md)

# onInputChanged

`suspend fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/awesomebar/src/main/java/mozilla/components/feature/awesomebar/provider/SearchSuggestionProvider.kt#L108)

Overrides [SuggestionProvider.onInputChanged](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/on-input-changed.md)

Fired whenever the user changes their input, after they have started interacting with the awesome bar.

This is a suspending function. An [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation is expected to invoke this method from a
[Coroutine](https://kotlinlang.org/docs/reference/coroutines-overview.html). This allows the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md)
implementation to group and cancel calls to multiple providers.

Coroutine cancellation is cooperative. A coroutine code has to cooperate to be cancellable:
https://github.com/Kotlin/kotlinx.coroutines/blob/master/docs/cancellation-and-timeouts.md

### Parameters

`text` - The current user input in the toolbar.

**Return**
A list of suggestions to be displayed by the [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md).

