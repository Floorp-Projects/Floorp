[android-components](../../../index.md) / [mozilla.components.lib.state](../../index.md) / [Store](../index.md) / [Subscription](index.md) / [resume](./resume.md)

# resume

`@Synchronized fun resume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L125)

Resumes the [Subscription](index.md). The [Observer](../../-observer.md) will get notified for every state change.
Additionally it will get invoked immediately with the latest state.

