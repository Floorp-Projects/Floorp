---
layout: page
title: Introduce a Store for UI components
permalink: /rfc/0012-introduce-ui-store
---

* Start date: 2024-01-29
* RFC PR: [#5353](https://github.com/mozilla-mobile/firefox-android/pull/5353)

## Summary

In most applications of the `Store`, it is preferable to have reducers perform work on the main thread. Having actions reduced immediately at the point of dispatch, simplifies the reasoning a developer would need to go through for most UI-based work that happens on the main thread.

## Motivation

Android embedders use the main thread for UI, user-facing, or gesture handling work. For example, notifying UI components when IO from storage layers have completed, an engine's task that can happen on a separate thread, or global-level state updates for different components to observe.

When components dispatch actions, they are performed on an independant single thread dispatcher in the `Store` to avoid overloading the main thread with heavy work that might be performed during the `reduce` or in a `Middleware`. In practice, these actions have been short and fast so they do not cause overhead (most of these actions have been [data class copying][0]).  In addition, side-effects done in a `Middleware` which can be slow, like I/O,  are put onto separate Dispatchers. The performance optimization to switch to a `Store` thread, requires that components which are always run on the main thread, to ensure synchronisation is now kept between the main thread and the store thread for observers of the `State`.

There are some advantages to this change:

* Simplicity for `Store`s that are meant for UI facing work.
* Unit testing can now occur on the test framework's thread.
* Fewer resources needed for context shifting between threads[^1].

For an example of thread simplicity, an `Engine` typically has its own 'engine thread' to perform async work and post/request results to the main thread (these APIs are identified with the `@UiThread` annotation). Once we get the callback for those results, we then need to dispatch an action to the store that will then happen on a `Store` thread. Feature components then observe for state changes and then make UI changes on the main thread. A simplified form of this thread context switching can be seen in the example below:

```kotlin
// engine thread
engineView.requestApiResult { result ->
  // received on the main thread.
  store.dispatch(UpdateResultAction(result))
}

// store thread
fun reduce(state: State, action: Action) {
  is UpdateResultAction -> {
    // do things here.
  }
}

// store thread
Middleware {
  override fun invoke(
    context: MiddlewareContext<State, Action>,
    next: (Action) -> Unit,
    action: Action,
  ) {
    // perform side-effects that also happen on the store thread.
  }
}

// main thread
store.flowScoped { flow ->
  flow.collect {
    // perform work on the main thread.
  }
}
```

With the changes in this RFC, this switching of threads can be reduced (notable comments marked with 📝):

```kotlin
// engine thread
engineView.requestApiResult { result ->
  // received on the main thread.
  store.dispatch(UpdateResultAction(result))
}

// 📝 main thread - now on the same thread, processed immediately.
fun reduce(state: State, action: Action) {
  is UpdateResultAction -> {
    // do things here.
  }
}

// 📝 main thread - now on the same thread, processed immediately.
Middleware {
  override fun invoke(
    context: MiddlewareContext<State, Action>,
    next: (Action) -> Unit,
    action: Action,
  ) {
    // 📝 perform side-effects that now happen on the main thread.
  }
}

// main thread
store.flowScoped { flow ->
  flow.collect {
      // perform work on the main thread.
    }
  }
}
```

Additionally, from [performance investigations already done][2], we know that Fenix creates over a hundred threads within a few seconds of startup. Reducing the number of threads for Stores that do not have a strong requirement to run on a separate thread will lower the applications memory footprint.

## Guide-level explanation

Extending the existing `Store` class to use the `Dispatchers.Main.immediate` will ensure that UI stores will stay on the same UI thread and have that work done immediately. Using a distinct class named `UiStore` also makes it clear to the developer that this is work that will be done on the UI thread and its implications will be made a bit more clear when it's used.

```kotlin
@MainThread
open class UiStore<S : State, A : Action>(
  initialState: S,
  reducer: Reducer<S, A>,
  middleware: List<Middleware<S, A>> = emptyList(),
) : Store<S, A>(
  initialState,
  reducer,
  middleware,
  UiStoreDispatcher(),
)

open class Store<S : State, A : Action> internal constructor(
  initialState: S,
  reducer: Reducer<S, A>,
  middleware: List<Middleware<S, A>>,
  dispatcher: StoreDispatcher,
) {
  constructor(
    initialState: S,
    reducer: Reducer<S, A>,
    middleware: List<Middleware<S, A>> = emptyList(),
    threadNamePrefix: String? = null,
  ) : this(
    initialState = initialState,
    reducer = reducer,
    middleware = middleware,
    dispatcher = DefaultStoreDispatcher(threadNamePrefix),
  )
}

interface StoreDispatcher {
  val dispatcher: CoroutineDispatcher
  val scope: CoroutineScope
  val coroutineContext: CoroutineContext

  // Each Store has it's own `assertOnThread` because in the Thread owner is different in both context.
  fun assertOnThread()
}
```

Applications can use this similar to any other store then. An "AppStore" example below can switch :

```kotlin
// changing the one line below from `UiStore` to `Store` gives the developer the ability to switch existing Stores between the different Store types.
class AppStore(
  initialState: AppState = AppState(),
) : UiStore<AppState, AppAction>(
  initialState = initialState,
  reducer = AppStoreReducer::reduce,
)
```

## Drawbacks

* Mistakenly doing work on the main thread - we could end up performing large amounts of work on the main thread unintentionally if we are not careful. This could be because of a large number of small tasks, a single large task, a blocking task, or a combination. As the developer is choosing to use a `UiStore`, they will be expected to ensure that heavy work they do, as is with mobile UI development done today, is not done on the main thread.

## Rationale and alternatives

Not introducing this new Store type would not change current development where the developer needs to ensure understanding that dispatched actions will be processed at a later time.

## Future work

We have opportunities to iterate from here and consider if/how we want to pass a CoroutineScope in. This can be part of future RFC proposals however.

## Unresolved questions

* While performance gains are not an explicit intent, there is a theoretical advantage, but not one we will pursue as part of this RFC. How much would we save, if any?
* Some additional changes need to be done to allow the `Store` to override the default `StoreThreadFactory` that will allow assertions against a thread (`MainThread`) not created by the `StoreThreadFactory` itself. This should be possible, but will this add to additional complexity?

[0]: https://kotlinlang.org/docs/data-classes.html#copying
[^1]: https://github.com/mozilla-mobile/android-components/issues/9424
[2]: https://github.com/mozilla-mobile/android-components/issues/9424#issue-787013588
