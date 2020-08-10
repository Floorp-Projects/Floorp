[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [warmUp](./warm-up.md)

# warmUp

`@MainThread open fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L69)

Makes sure all required engine initialization logic is executed. The
details are specific to individual implementations, but the following must be true:

* The engine must be operational after this method was called successfully
* Calling this method on an engine that is already initialized has no effect
