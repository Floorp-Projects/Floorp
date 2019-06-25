[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [Store](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Store(initialState: `[`S`](index.md#S)`, reducer: `[`Reducer`](../-reducer.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>)`

A generic store holding an immutable [State](../-state.md).

The [State](../-state.md) can only be modified by dispatching [Action](../-action.md)s which will create a new state and notify all registered
[Observer](../-observer.md)s.

### Parameters

`initialState` - The initial state until a dispatched [Action](../-action.md) creates a new state.

`reducer` - A function that gets the current [State](../-state.md) and [Action](../-action.md) passed in and will return a new [State](../-state.md).