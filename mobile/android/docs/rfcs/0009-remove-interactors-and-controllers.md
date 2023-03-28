---
layout: page
title: Remove Interactors and Controllers
permalink: /rfc/0009-remove-interactors-and-controllers
---

* Start date: 2022-03-29
* RFC PR: [1466](https://github.com/mozilla-mobile/firefox-android/pull/1466)

## Summary

Now that Fenix has been fully migrated to rely on Android Component's `lib-state` library, Fenix's architecture can be further simplified by refactoring view components to directly use our Redux-like store, instead of going through various layers of `Interactor`s and `Controller`s.

## Motivation

The `Interactor/Controller` types, as a design pattern, have recently had their usefulness and clarity come into question in reviews and conversations with some regularity. `Interactor`s in particular seem to often be an unnecessary abstraction layer, usually containing methods that call similarly named methods in a `Controller` that share a similar name to the interactor calling them. Interactors that are used solely to delegate to other classes will be referred to as 'passthrough' interactors throughout this document. 

The [definition](../../fenix/docs/architecture-overview.md#interactor) of interactors in our architecture overview indicate that this is at least partially intentional: 'Called in response to a direct user action. Delegates to something else'. This is even referenced as a [limitation at the end of the overview](../../fenix/docs/architecture-overview.md#known-limitations). 

Historically, the interactor/controller pattern evolved from a Presenter/Controller/View pattern that originated in Android Components. The underlying motivation of that pattern was to separate code presenting the state from the view (Presenters) and the code that updated the state from the view (Controllers).

Generally, the interactor/controller pattern is also intended to move business logic out of classes tied to the Android framework and into classes that are more testable, re-usable, composable, digestible, and which handle a single responsibility. More reading on architecture goals is available [in our Architecture Decisions](../../fenix/docs/Architecture-Decisions.md#goals).

These goals are all met reasonably well by the existing pattern, but it also contributes to unnecessary class explosion, general code complexity, and confusion about responsibility for each architectural type. This becomes especially true when wondering how responsibility should be split between interactors and controllers. It seems that interactors are often included as a matter of precedent and not to necessarily facilitate all the goals mentioned above, and this has lead to a large amount passthrough interactors.

### Proposal goals

1. Increase code comprehensibility
2. Have clear delineation of responsibility between architectural components
3. Retain ability to manage state outside of fragments/activities/etc
4. Ability for proposal to be adopted incrementally

### Further contextual reading

An investigation was previously done to provide [further context](https://docs.google.com/document/d/1vwERcAW9_2LkcYENhnA5Kb-jT_nBTJNwnbD5m9zYSA4/edit#heading=h.jjoifpwhhxgk) on the current state of interactors. To summarize findings _in Fenix specifically_:
1. 29/36 concrete `Interactor` classes are strict passthrough interactors or are passthrough interactors that additionally record metrics.
2. Other interactors seem to have mixed responsibilities, including but not limited to:
	- dispatching actions to `Store`s
	- initiating navigation events
	- delegating to `UseCase`s or directly to closures instead of `Controller`s.

## Guide-level explanation

The proposal is to remove interactors and controllers completely from the codebase. Their usages will be replaced with direct Store observations and direct action dispatches to those Stores. All state changes would be handled by reducers, and side-effects like telemetry or disk writes would be handled in middlewares.

This would address all the goals listed above:
1. Code comprehensibility should be improved by having a single architectural pattern. All business logic would be discoverable within `Reducer`s, and changes to `State` would be much more explicit. 
2. Responsibility would be clearly delineated as all business logic would be handled by `Reducer`s and all side-effects by `Middleware`, instead of being scattered between those components as well as `Interactor`s, `Controller`s, and various utility classes.
3. `State` management would happen within `Reducer`s and would still accomplish the usual goals around testability.
4. Refactoring interactors/controllers to instead dispatch actions and react to state changes should be doable on a per-component basis.

Additionally, there is tons of prior art discussing the benefits of a unidirectional data flow, including the [Redux documentation](https://redux.js.org/tutorials/fundamentals/part-2-concepts-data-flow).

## Reference-level explanation

Throughout the app are examples of chains of interactors and controllers which lead to a method that dispatches events to a Store anyway. Simplifying this mental model should allow faster code iteration and code navigation.

For one example, here is the code path that happens when a user long-clicks a history item:

```kotlin
// 1. in LibrarySiteItemView
setOnClickListener {
    val selected = holder.selectedItems
    when {
        selected.isEmpty() -> interactor.open(item)
        item in selected -> interactor.deselect(item)
        // "select" is the path we are following
        else -> interactor.select(item)
    }
}

// 2. Clicking into `interactor.select` will lead to SelectionInteractor<T>.
// 3. Searching for implementors of that interface will lead us to another interface, `HistoryInteractor : SelectionInteractor<History>`
// 4. DefaultHistoryInteractor implements HistoryInteractor
override fun open(item: History) {
    historyController.handleSelect(item)
}

// 5. Clicking into `handleSelect` leads to `HistoryController`
// 6. DefaultHistoryController implements `HistoryController::handleSelect`
override fun handleSelect(item: History) {
    if (store.state.mode === HistoryFragmentState.Mode.Syncing) {
        return
    }

    store.dispatch(HistoryFragmentAction.AddItemForRemoval(item))
}

// 7. reducer handles state update
private fun historyStateReducer(
    state: HistoryFragmentState,
    action: HistoryFragmentAction,
): HistoryFragmentState {
    return when (action) {
        is HistoryFragmentAction.AddItemForRemoval ->
            state.copy(mode = HistoryFragmentState.Mode.Editing(state.mode.selectedItems + action.item))
        ...
    }
}
```

Following this proposal, the above would change to something like:

```kotlin
// 1. in LibrarySiteItemView
setOnClickListener {
    when {
        selected.isEmpty() -> store.dispatch(HistoryFragmentAction.AddItemForRemoval(item))
        ...
    }
}

// 2. reducer handles state
private fun historyStateReducer(
    state: HistoryFragmentState,
    action: HistoryFragmentAction,
): HistoryFragmentState {
    return when (action) {
        is HistoryFragmentAction.AddItemForRemoval(item) -> when (state.mode) {
            HistoryFragmentState.Mode.Syncing -> state
            else -> state.copy(
                mode = HistoryFragmentState.Mode.Editing(state.mode.selectedItems + action.item)
            )
        }
        ...
    }
}
```

This reduces the number of layers in the code path. Note also that business logic around whether to update based on Sync status is incorporated locally in the Reducer. In this example, the details of observing state from the store are complicated by the additional view hierarchies required by recycler views, so they have been omitted for brevity.

### Moving into the future: using lib-state with Compose

Ideally, fragments or top-level views/Composables would register observers of their Store's state and send updates from those observers to their child Composables along with closures containing dispatches to those Stores. We are already close to being familiar with this pattern in some places. The following is a current example that has been edited for brevity and clarity:

```kotlin
class PocketCategoriesViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: PocketStoriesInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    @Composable
    override fun Content() {
        val categories = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategories }.value
        val categoriesSelections = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategoriesSelections }.value

        PocketTopics(
            categoryColors = categoryColors,
            categories = categories ?: emptyList(),
            categoriesSelections = categoriesSelections ?: emptyList(),
            onCategoryClick = interactor::onCategoryClicked,
        )
    }
}
```

Here, we already see a view registering Store observers appropriately. The only thing that would need to change is the line calling the interactor, `onCategoryClick = interactor::onCategoryClicked,`. Following this line leads down a similar chain of abstraction layers, and eventually leads to `DefaultPocketStoriesController::handleCategoryClicked`. This function contains business logic which would need to be moved to the reducer and telemetry side-effects that would need to be moved to a middleware, but the ultimate intent to to toggle the selected category. We skip many layers of indirection by dispatching that action directly from the view:

```kotlin
class PocketCategoriesViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    @Composable
    override fun Content() {
        val categories = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategories }.value
        val categoriesSelections = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategoriesSelections }.value

        PocketTopics(
            categoryColors = categoryColors,
            categories = categories ?: emptyList(),
            categoriesSelections = categoriesSelections ?: emptyList(),
            onCategoryClick = { name -> 
                components.appStore.dispatch(AppAction.TogglePocketStoriesCategory(name)) 
            },
        )
    }
}
```

This should simplify the search for underlying logic by:
- moving business logic into an expected and consistent component (the reducer)
- moving side-effects into a consistent and centralized component (a middleware, which could even handle telemetry for the entire store)

## Drawbacks

Redux-like patterns have a bit of a learning curve, and some problems can be more difficult to solve than others. For example, handling side-effects like navigation or loading state from disk. The benefits of streamlining our architecture should outweigh this, especially once demonstrative examples of solving these problems are common in the codebase.
