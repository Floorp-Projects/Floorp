[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [clearSpeculativeSession](./clear-speculative-session.md)

# clearSpeculativeSession

`@MainThread open fun clearSpeculativeSession(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L137)

Removes and closes a speculative session created by [speculativeCreateSession](speculative-create-session.md). This is
useful in case the session should no longer be used e.g. because engine settings have
changed.

