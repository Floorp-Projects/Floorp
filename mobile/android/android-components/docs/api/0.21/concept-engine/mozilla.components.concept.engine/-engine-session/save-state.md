---
title: EngineSession.saveState - 
---

[mozilla.components.concept.engine](../index.html) / [EngineSession](index.html) / [saveState](./save-state.html)

# saveState

`abstract fun saveState(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`

Saves and returns the engine state. Engine implementations are not required
to persist the state anywhere else than in the returned map. Engines that
already provide a serialized state can use a single entry in this map to
provide this state. The only requirement is that the same map can be used
to restore the original state. See [restoreState](restore-state.html) and the specific
engine implementation for details.

