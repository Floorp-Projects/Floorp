[android-components](../../index.md) / [mozilla.components.browser.awesomebar.transform](../index.md) / [SuggestionTransformer](./index.md)

# SuggestionTransformer

`interface SuggestionTransformer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/transform/SuggestionTransformer.kt#L15)

A [SuggestionTransformer](./index.md) takes an input list of [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) and returns a new list of transformed
[AwesomeBar.Suggestion| objects.

A [SuggestionTransformer](./index.md) can be used to adding data, removing data or filtering suggestions:

### Functions

| Name | Summary |
|---|---|
| [transform](transform.md) | `abstract fun transform(provider: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`, suggestions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`>`<br>Takes a list of [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) object and the [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) instance that emitted the suggestions in order to return a transformed list of [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) objects. |
