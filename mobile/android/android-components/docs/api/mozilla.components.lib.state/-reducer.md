[android-components](../index.md) / [mozilla.components.lib.state](index.md) / [Reducer](./-reducer.md)

# Reducer

`typealias Reducer<S, A> = (`[`S`](-reducer.md#S)`, `[`A`](-reducer.md#A)`) -> `[`S`](-reducer.md#S) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L24)

Reducers specify how the application's [State](-state.md) changes in response to [Action](-action.md)s sent to the [Store](-store/index.md).

Remember that actions only describe what happened, but don't describe how the application's state changes.
Reducers will commonly consist of a `when` statement returning different copies of the [State](-state.md).

