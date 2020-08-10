[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [RunWhenReadyQueue](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`RunWhenReadyQueue(scope: CoroutineScope = MainScope())`

A queue that acts as a gate, either executing tasks right away if the queue is marked as "ready",
i.e. gate is open, or queues them to be executed whenever the queue is marked as ready in the
future, i.e. gate becomes open.

