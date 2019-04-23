[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [Observer](index.md) / [onLoadRequest](./on-load-request.md)

# onLoadRequest

`open fun onLoadRequest(triggeredByUserInteraction: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L60)

The engine received a request to load a request.

### Parameters

`triggeredByUserInteraction` - True if and only if the request was triggered by user interaction (e.g.
clicking on a link on a website).