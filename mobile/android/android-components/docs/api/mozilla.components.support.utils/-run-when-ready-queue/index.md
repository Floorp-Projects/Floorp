[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [RunWhenReadyQueue](./index.md)

# RunWhenReadyQueue

`class RunWhenReadyQueue` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/RunWhenReadyQueue.kt#L17)

A queue that acts as a gate, either executing tasks right away if the queue is marked as "ready",
i.e. gate is open, or queues them to be executed whenever the queue is marked as ready in the
future, i.e. gate becomes open.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RunWhenReadyQueue(scope: CoroutineScope = MainScope())`<br>A queue that acts as a gate, either executing tasks right away if the queue is marked as "ready", i.e. gate is open, or queues them to be executed whenever the queue is marked as ready in the future, i.e. gate becomes open. |

### Functions

| Name | Summary |
|---|---|
| [isReady](is-ready.md) | `fun isReady(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Was this queue ever marked as 'ready' via a call to [ready](ready.md)? |
| [ready](ready.md) | `fun ready(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Mark queue as ready. Pending tasks will execute, and all tasks passed to [runIfReadyOrQueue](run-if-ready-or-queue.md) after this point will be executed immediately. |
| [runIfReadyOrQueue](run-if-ready-or-queue.md) | `fun runIfReadyOrQueue(task: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs the [task](run-if-ready-or-queue.md#mozilla.components.support.utils.RunWhenReadyQueue$runIfReadyOrQueue(kotlin.Function0((kotlin.Unit)))/task) if this queue is marked as ready, or queues it for later execution. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
