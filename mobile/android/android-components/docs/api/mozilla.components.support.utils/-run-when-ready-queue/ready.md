[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [RunWhenReadyQueue](index.md) / [ready](./ready.md)

# ready

`fun ready(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/RunWhenReadyQueue.kt#L50)

Mark queue as ready. Pending tasks will execute, and all tasks passed to [runIfReadyOrQueue](run-if-ready-or-queue.md)
after this point will be executed immediately.

