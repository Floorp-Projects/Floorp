[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [SuggestionProvider](index.md) / [onInputChanged](./on-input-changed.md)

# onInputChanged

`abstract suspend fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../-suggestion/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L160)

Fired whenever the user changes their input, after they have started interacting with the awesome bar.

This is a suspending function. An [AwesomeBar](../index.md) implementation is expected to invoke this method from a
[Coroutine](https://kotlinlang.org/docs/reference/coroutines-overview.html). This allows the [AwesomeBar](../index.md)
implementation to group and cancel calls to multiple providers.

Coroutine cancellation is cooperative. A coroutine code has to cooperate to be cancellable:
https://github.com/Kotlin/kotlinx.coroutines/blob/master/docs/cancellation-and-timeouts.md

### Parameters

`text` - The current user input in the toolbar.

**Return**
A list of suggestions to be displayed by the [AwesomeBar](../index.md).

