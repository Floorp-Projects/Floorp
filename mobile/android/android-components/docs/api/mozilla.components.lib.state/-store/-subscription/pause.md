[android-components](../../../index.md) / [mozilla.components.lib.state](../../index.md) / [Store](../index.md) / [Subscription](index.md) / [pause](./pause.md)

# pause

`@Synchronized fun pause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L136)

Pauses the [Subscription](index.md). The [Observer](../../-observer.md) will not get notified when the state changes
until [resume](resume.md) is called.

