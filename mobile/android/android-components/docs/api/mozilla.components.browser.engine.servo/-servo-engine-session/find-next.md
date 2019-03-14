[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [findNext](./find-next.md)

# findNext

`fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L134)

Overrides [EngineSession.findNext](../../mozilla.components.concept.engine/-engine-session/find-next.md)

Finds and highlights the next or previous match found by [findAll](../../mozilla.components.concept.engine/-engine-session/find-all.md).

### Parameters

`forward` - true if the next match should be highlighted, false for
the previous match.