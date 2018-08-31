---
title: GeckoEngineSession.saveState - 
---

[mozilla.components.browser.engine.gecko](../index.html) / [GeckoEngineSession](index.html) / [saveState](./save-state.html)

# saveState

`fun saveState(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`

See [EngineSession.saveState](#)

GeckoView provides a String representing the entire session state. We
store this String using a single Map entry with key GECKO_STATE_KEY.

See https://bugzilla.mozilla.org/show_bug.cgi?id=1441810 for
discussion on sync vs. async, where a decision was made that
callers should provide synchronous wrappers, if needed. In case we're
asking for the state when persisting, a separate (independent) thread
is used so we're not blocking anything else. In case of calling this
method from onPause or similar, we also want a synchronous response.

