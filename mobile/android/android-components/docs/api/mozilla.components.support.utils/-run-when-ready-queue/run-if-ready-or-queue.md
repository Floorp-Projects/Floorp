[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [RunWhenReadyQueue](index.md) / [runIfReadyOrQueue](./run-if-ready-or-queue.md)

# runIfReadyOrQueue

`fun runIfReadyOrQueue(task: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/RunWhenReadyQueue.kt#L36)

Runs the [task](run-if-ready-or-queue.md#mozilla.components.support.utils.RunWhenReadyQueue$runIfReadyOrQueue(kotlin.Function0((kotlin.Unit)))/task) if this queue is marked as ready, or queues it for later execution.

### Parameters

`task` - : The task to run now if queue is ready or queue for later execution.