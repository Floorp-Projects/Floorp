[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [recoverFromCrash](./recover-from-crash.md)

# recoverFromCrash

`abstract fun recoverFromCrash(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L471)

Tries to recover from a crash by restoring the last know state.

Returns true if a last known state was restored, otherwise false.

