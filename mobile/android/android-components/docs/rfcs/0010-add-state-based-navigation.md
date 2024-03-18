---
layout: page
title: Add State-based navigation
permalink: /rfc/0010-add-state-based-navigation
---

* Start date: 2023-10-17
* RFC PR: [4126](https://github.com/mozilla-mobile/firefox-android/pull/4126)

## Summary

Following the acceptance of [0009 - Remove Interactors and Controllers](0009-remove-interactors-and-controllers), Fenix should have a method of navigation that is tied to the `lib-state` model to  provide a method of handling navigation side-effects that is consistent with architectural goals. This architecture is defined at a high-level [here](https://github.com/mozilla-mobile/firefox-android/blob/9edfde0a1382b4d5fb4342792d98d4c1d4d41bef/fenix/docs/architecture-overview.md) and has example code in [this folder](https://github.com/mozilla-mobile/firefox-android/tree/9edfde0a1382b4d5fb4342792d98d4c1d4d41bef/fenix/docs/architectureexample).

## Motivation

Currently, methods of navigation throughout the app are varied. The `SessionControlController` provides [3 examples](https://searchfox.org/mozilla-mobile/rev/aa6bee71a6e0ea73f041a54ddf4d5d4e2f603429/firefox-android/fenix/app/src/main/java/org/mozilla/fenix/home/sessioncontrol/SessionControlController.kt#180) alone:

- `HomeActivity::openToBrowserAndLoad`
- Calling a `NavController` directly
- Callbacks like `showTabTray()`

To move to a more consistent Redux-like model, we need a way for features to fire `Action`s and have that result in navigation. This will help decouple our business logic from the Android platform, where a key example of this would be references to the `HomeActivity` throughout the app in order to access the `openToBrowserAndLoad` function.

## Proposal

Moving forward, navigation should be initiated through middlewares that respond to `Action`s. How these middleware handle navigation side-effects can be addressed on a per-case basis, but this proposal includes some generalized advice for 3 common use cases.

### 1. Screen-based navigation

For screen-based navigation between screens like the settings pages or navigation to the home screen, middlewares should make direct use of a `NavController` that is hosted by the fragment of the current screen's scope.

For a hypothetical example:
```kotlin

sealed class HistoryAction {
    object HomeButtonClicked : HistoryAction()
    data class HistoryGroupClicked(val group: History.Group) : HistoryAction()
}

class HistoryNavigationMiddleware(private val getNavController: () -> NavController) : Middleware<HistoryState, HistoryAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryState, HistoryAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HomeButtonClicked -> getNavController().navigate(R.id.home_fragment)
            is HistoryGroupClicked -> getNavController().navigate(R.id.history_metadata_fragment)
        }
    }
}
```

This should translate fairly easily to the Compose world. This example intentionally ignores passing the `group` through the navigation transition. It should be fairly trivial to convert data types to navigation arguments, or consider creating Stores with a scope large enough to maintain state across these transitions.

Note also the use of a lambda to retrieve the `NavController`. This should help avoid stale references when Stores outlive their parent fragment by using a `StoreProvider`.

### 2. Transient effects

Transient effects can be handled by callbacks provided to a middleware. To build on our previous example:

```kotlin
sealed class HistoryAction {
    object HomeButtonClicked : HistoryAction()
    data class HistoryGroupClicked(val group: HistoryItem.Group) : HistoryAction()
    data class HistoryItemLongClicked(val item: HistoryItem) : HistoryAction()
}

class HistoryUiEffectMiddleware(
    private val displayMenuForItem: (HistoryItem) -> Unit,
) : Middleware<HistoryState, HistoryAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryState, HistoryAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HistoryItemLongClicked -> displayMenuForItem(action.item)
            is HomeButtonClicked, HistoryGroupClicked -> Unit
        }
    }
}
```

### 3. The special case of `openToBrowserAndLoad`

Finally, we want a generally re-usable method of opening a new tab and navigating to the `BrowserFragment`. Fragment-based Stores can re-use a (theoretical) delegate to do so.

```kotlin
sealed class HistoryAction {
    object HomeButtonClicked : HistoryAction()
    data class HistoryGroupClicked(val group: History.Group) : HistoryAction()
    data class HistoryItemLongClicked(val item: HistoryItem) : HistoryAction()
    data class HistoryItemClicked(val item: History.Item) : HistoryAction()
}

class HistoryNavigationMiddleware(
    private val browserNavigator: BrowserNavigator,
    private val getNavController: () -> NavController,
) : Middleware<HistoryState, HistoryAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryState, HistoryAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HomeButtonClicked -> navController.navigate(R.id.home_fragment)
            is HistoryGroupClicked -> navController.navigate(R.id.history_metadata_fragment)
            is HistoryItemClicked -> browserNavigator.openToBrowserAndLoad(action.item)
            is HistoryItemLongClicked -> Unit
        }
    }
}
```

This delegate would wrap the current behavior exposed by `HomeActivity::openToBrowserAndLoad`, looking something roughly like:

```kotlin
class BrowserNavigator(
    private val addTabUseCase: AddNewTabUseCase,
    private val loadTabUseCase: DefaultLoadUrlUseCase,
    private val searchUseCases: SearchUseCases,
    private val navController: () -> NavController,
) {
    // logic to navigate to browser fragment and load a tab
}
```

## Alternatives

### 1. Observing navigation State from a AppStore through a binding in HomeActivity.

This was the previous proposal for this RFC. An example would roughly be:

```kotlin
sealed class AppAction {
    object NavigateHome : AppAction()
}

data class AppState(
    val currentScreen: Screen
)

fun appReducer(state: AppState, action: AppAction): AppState = when (action) {
    is NavigateHome -> state.copy(currentScreen = Screen.Home)
}

// in HomeActivity
private val navigationObserver by lazy {
    object : AbstractBinding<AppState>(components.appStore) {
        override suspend fun onState(flow: Flow<AppState>) = flow
            .distinctUntilChangedBy { it.screen }
            .collectLatest { /* handleNavigation */ }
    }
}
```

However, this implies some several issues:
1. We end up replicating the state of a `NavController` manually in a our custom State, risking out-of-sync issues.
2. We lose specificity of Actions by generalizing them globally. For example, instead of a `ToolbarAction.HomeClicked`, it would encourage re-use of a single `AppAction.NavigateHome`. Though seemingly convenient at first, it implies downstream problems for things like telemetry. To know where the navigation to home originated from, we would need to include additional properties (like direction) in the `Action`. Any future changes to the behavior of these Actions would need to be generalized for the whole app.

### 2. Global navigation middleware attached to the AppStore.

This carries risk of the 2 issue listed above, and runs into immediate technical constraints. When the `AppStore` is constructed in `Core`, we do not have reference to an `Activity` and cannot retrieve a `NavController`. This could be mitigated by a mutable property or lazy getter that is set as Fragments or the Activity come into and out of scope. The current proposal will localize navigation transitions to feature areas which should keep them isolated in scope.
