---
title: EngineSession.restoreState - 
---

[mozilla.components.concept.engine](../index.html) / [EngineSession](index.html) / [restoreState](./restore-state.html)

# restoreState

`abstract fun restoreState(state: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Restores the engine state as provided by [saveState](save-state.html).

### Parameters

`state` - state retrieved from [saveState](save-state.html)