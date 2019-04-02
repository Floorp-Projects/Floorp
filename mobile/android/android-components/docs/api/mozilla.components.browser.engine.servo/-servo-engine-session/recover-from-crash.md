[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [recoverFromCrash](./recover-from-crash.md)

# recoverFromCrash

`fun recoverFromCrash(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L146)

Overrides [EngineSession.recoverFromCrash](../../mozilla.components.concept.engine/-engine-session/recover-from-crash.md)

Tries to recover from a crash by restoring the last know state.

Returns true if a last known state was restored, otherwise false.

