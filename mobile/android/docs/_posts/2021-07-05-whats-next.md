---
layout: post
title: "⏭️ After the browser-state migration - what's next?"
date: 2021-07-05 14:00:00 +0200
author: sebastian
---

After 2+ years slowly and incrementally working towards this goal, we [completed the migration](https://github.com/mozilla-mobile/android-components/pull/10436) from the `browser-session` component to the `browser-state` component for state handling. Finally, we were able to delete `browser-session` and all state is now maintained and updated by the [redux](https://redux.js.org/introduction/core-concepts)-like [`BrowserStore`](https://github.com/mozilla-mobile/android-components/blob/main/components/browser/state/src/main/java/mozilla/components/browser/state/store/BrowserStore.kt#L22).

The following blog posting describes some of the possible follow-up changes to the architecture that we would consider, depending on the outcome of further discussions and prototyping.

## Reversing the dependency between browser-state and browser-engine implementations

In the current architecture every `browser-engine` implements `concept-engine` and exposes an abstracted mechanism for observing events. For every `EngineSession` an `EngineObserver` gets created, which will dispatch a `BrowserAction` for every event, updating the centralized state.

![](/assets/images/blog/engine-architecture.png)

Now that the migration to `browser-state` is completed, we can reverse this dependency. With a `browser-engine` implementation depending on `browser-state` directly, it can dispatch actions without an observer in between.

![](/assets/images/blog/simplified-engine-architecture.png)

This removes the requirement for a shared, abstracted observer interface in `concept-engine`. It would no longer be required to have shared _“glue code”_ for connecting an abstract browser engine with the state handling component.

A potential downside to this approach is that the `BrowserStore` and related `BrowserAction`s become the new _“interface”_ that a browser engine has to dispatch correctly, which can be harder to understand and follow than simply implementing actual interfaces.

Overall this seems to be worthwhile exploring and potentially discussing further in an RFC.

## Jetpack Compose

Android’s new UI toolkit, [Jetpack Compose](https://developer.android.com/jetpack/compose), will significantly change how we build user-facing features in components and apps.

The good news is that Jetpack Compose works very nicely with our browser-state component. In Android Components we will provide bindings that allow subscribing to any `lib-state` baked `Store`, causing a recomposition if the observed state changes.

```kotlin
@Composable
fun SimpleToolbar(
    store: BrowserStore
) {
    // Subscribe to the URL of the selected tab
    val url = store.observeAsState { state -> state.selectedTab?.content?.url }

    // Will automatically get recomposed if the URL changes
    Text(url.value ?: "")
}
```

### Observing specific tabs

Today we have many components that optionally take a nullable `tabId: String?` parameter. If a tab ID is provided then the component is supposed to observe this specific tab. And if the parameter is `null` then the component will automatically track the currently selected tab. This has caused issues in the past when a `null` value was provided accidentally, causing the wrong tab to be tracked. With Jetpack Compose we want to make this more explicit and will provide a `Target` class, that lets the caller explicily define what tab should be targeted. In addition to that this allows us to provide extension functions for easily observing this tab.

```kotlin
@Composable
fun Example(store: BrowserStore) {
    // Explicitly observe specific tabs
    SimpleToolbar(store, Target.SelectedTab)
    SimpleToolbar(store, Target.Tab("tabId"))
    SimpleToolbar(store, Target.CustomTab("customTabId"))
}

@Composable
fun SimpleToolbar(
    store: BrowserStore,
    target: Target
) {
    // Observe the URL of the target. Only when the URL changes, this will
    // cause a recomposition. Other changes of the tab get ignored.
    val tab: SessionState? by target.observeAsStateFrom(
        store = store,
        observe = { tab -> tab?.content?.url }
    )

    Text(tab?.content?.url ?: "")
}
```

## Observing state directly vs concept components

A central piece of our [current component architecture](/contributing/architecture) is splitting the implementation into three pieces: A concept component, an implementation, and a glue/feature component for integration. This allows us to easily swap (concept) implementations, without having to change any other code. The downside of this approach is that all implementations need to abide by the interface abstractions.

Let’s look at the toolbar component as an example.

![](/assets/images/blog/toolbar-architecture.png)

* `concept-toolbar` contains the interface and data classes to describe a toolbar and how other components can interact with it.
* `browser-toolbar` is an implementation of concept-toolbar.
* `feature-toolbar` contains the glue code, subscribing to state updates in order to update a toolbar (presenter), and reacting to toolbar events in order to invoke use cases (interactor).

Using the two concepts above, reversing the dependency and using Jetpack Compose, we can simplify this architecture and reduce it to a single component. A (UI) component written in Jetpack Compose can directly observe `BrowserStore` for state updates and delegate events to a function callback parameter or `UseCase` class directly.

![](/assets/images/blog/simplified-toolbar-architecture.png)

## Differently scoped states

Our browser applications maintain state adhering to three different scopes: browser state, screen state and app state.

### Browser State

_"Browser State"_ is the state the browser is in (e.g. “which tabs are open?”) and the state that is shared with our Android Components. It’s available through the `browser-state` component to other components and the application.

With the bindings mentioned above, `browser-state` works well with Jetpack Compose.

### Screen State

_"Screen State"_ is the state for the currently displayed screen (e.g. “what text is the user entering on the search screen?”). In Firefox for Android, we are using `lib-state` backed stores for each screen (e.g. `SearchFragmentStore`).

As for `browser-state` above, with the Jetpack Compose bindings for every `lib-state` implementation, we can continue to use our existing screen-scoped stores.

Alternatives, used by the Android community, are [`ViewModel`](https://developer.android.com/reference/kotlin/androidx/lifecycle/ViewModel)s using [`LiveData`](https://developer.android.com/reference/androidx/lifecycle/LiveData) or [`StateFlow`](https://kotlin.github.io/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines.flow/-state-flow/). Or, at a lower level, Jetpack Compose idioms like [`rememberSaveable()`](https://developer.android.com/reference/kotlin/androidx/compose/runtime/saveable/package-summary#rememberSaveable(kotlin.Array,androidx.compose.runtime.saveable.Saver,kotlin.String,kotlin.Function0)).

Currently we do not offer bindings in `lib-state` for saving and restoring state to survive activity or process recreation. This is something we could add (based on [`Saver`](https://developer.android.com/reference/kotlin/androidx/compose/runtime/saveable/Saver)), specifically for scoped stores.

```kotlin
val store = scopedSaveableStore<BrowserScreenState, BrowserScreenAction> { restoredState ->
    BrowserScreenStore(restoredState ?: BrowserScreenState())
}
```

### App State

_"App state"_ is the state the app is in, independently from the currently displayed screen (e.g. “Is the app in light or dark mode?”).

Currently in Fenix there’s no centralized app state. For some parts there are manager singletons (e.g. `ThemeManager`) or the state is read from `SharedPreferences`.

We could try using an global app store and the same patterns we use for the browser store and the screen scoped stores. That’s something we are currently trying in Focus ([AppStore](https://github.com/mozilla-mobile/focus-android/blob/main/app/src/main/java/org/mozilla/focus/state/AppStore.kt)). In fact, in Focus, the screen scoped state is a sub state of the application-wide state ([Screen state](https://github.com/mozilla-mobile/focus-android/blob/main/app/src/main/java/org/mozilla/focus/state/AppState.kt#L26)).

### Stateless composables

With Jetpack Compose it is preferred to write stateless composables ([State hoisting](https://developer.android.com/jetpack/compose/state#state-hoisting)). This means that state is passed down as function parameters and events are passed up by invoking functions. Listening to the store and dispatching actions sidesteps this mechanism. Only subscribing to state changes at the top layer and passing everything down/up is cumbersome and may introduce a lot of duplicated “glue” code across our projects.

Let’s look at a simplified example of a browser toolbar. There are two options:

**Example A**: All state (e.g. the URL to display) gets passed down to the toolbar:

```Kotlin
@Composable
fun Toolbar(
    url: String
) {
    Text(url)
}
```

**Example B**: The toolbar subscribes to state it needs itself.

```Kotlin
@Composable
fun Toolbar(
    store: BrowserStore
) {
    val url = store.observeAsState { state -> state.selectedTab?.content?.url }
    Text(url.value ?: "")
}
```

The code from example **A** is the most reusable. The app is in full control of what state gets displayed. But this also introduces duplicate code across apps (for getting the state and passing it down) and makes it more likely to introduce bugs (security and spoofing). Example **B** is guaranteed to be consistent across apps. But the composable is strictly tied to the state and how it gets observed.

When writing (UI) components using Jetpack Compose, we will have to find the right balance between the two patterns.

In the best case the composition of composables make both patterns possible, depending on the needs of the component consumer:

```kotlin
@Composable
fun Toolbar(
    store: BrowserStore
) {
    val url = store.observeAsState { state -> state.selectedTab?.content?.url }
    Toolbar(url.value ?: "")
}

@Composable
fun Toolbar(
    url: String
) {
    Text(url)
}
```
