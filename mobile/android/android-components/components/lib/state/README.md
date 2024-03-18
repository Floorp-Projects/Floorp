# [Android Components](../../../README.md) > Libraries > State

A generic library for maintaining the state of a component, screen or application.

The state library is inspired by existing libraries like [Redux](https://redux.js.org/) and provides a `Store` class to hold application state.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-state:{latest-version}"
```

### Action

`Action`s represent payloads of information that send data from your application to the `Store`. You can send actions using `store.dispatch()`. An `Action` will usually be a small data class or object describing a change.

```Kotlin
data class SetVisibility(val visible: Boolean) : Action

store.dispatch(SetVisibility(true))
```

### Reducer

`Reducer`s are functions describing how the state should change in response to actions sent to the store.

They take the previous state and an action as parameters, and return the new state as a result of that action.

```Kotlin
fun reduce(previousState: State, action: Action) = when (action) {
    is SetVisibility -> previousState.copy(toolbarVisible = action.visible)
    else -> previousState
}
```

### Store

The `Store` brings together actions and reducers. It holds the application state and allows access to it via the `store.state` getter. It allows state to be updated via `store.dispatch()`, and can have listeners registered through `store.observe()`.

Stores can easily be created if you have a reducer.

```Kotlin
val store = Store<State, Action>(
    initialState = State(),
    reducer = ::reduce
)
```

Once the store is created, you can react to changes in the state by registering an observer.

```Kotlin
store.observe(lifecycleOwner) { state ->
    toolbarView.visibility = if (state.toolbarVisible) View.VISIBLE else View.GONE
}
```

`store.observe` is lifecycle aware and will automatically unregister when the lifecycle owner (such as an `Activity` or `Fragment`) is destroyed. Instead of a `LifecycleOwner`, a `View` can be supplied instead.

If you wish to manually control the observer subscription, you can use the `store.observeManually` function. `observeManually` returns a `Subscription` class which has an `unsubscribe` method. Calling `unsubscribe` removes the observer.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
