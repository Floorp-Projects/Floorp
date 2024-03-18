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

Generally, the interactor/controller pattern is also intended to move business logic out of classes tied to the Android framework and into classes that are more testable, reusable, composable, digestible, and which handle a single responsibility. More reading on architecture goals is available [in our Architecture Decisions](../../fenix/docs/Architecture-Decisions.md#goals).

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

---

## Guide-level explanation

The proposal is to remove interactors and controllers completely from the codebase. Their usages will be replaced with direct Store observations and direct action dispatches to those Stores. All state changes would be handled by reducers, and side-effects like telemetry or disk writes would be handled in middlewares.

This would address all the goals listed above:
1. Code comprehensibility should be improved by having a single architectural pattern. All business logic would be discoverable within `Reducer`s, and changes to `State` would be much more explicit.
2. Responsibility would be clearly delineated as all business logic would be handled by `Reducer`s and all side-effects by `Middleware`, instead of being scattered between those components as well as `Interactor`s, `Controller`s, and various utility classes.
3. `State` management would happen within `Reducer`s and would still accomplish the usual goals around testability.
4. Refactoring interactors/controllers to instead dispatch actions and react to state changes should be doable on a per-component basis.

Additionally, there are tons of prior art discussing the benefits of a unidirectional data flow, including the [Redux documentation](https://redux.js.org/tutorials/fundamentals/part-2-concepts-data-flow).

---

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
// 3. Searching for implementers of that interface will lead us to another interface, `HistoryInteractor : SelectionInteractor<History>`
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

---

### Reacting to changes: state observations and side-effects in XML

Generally speaking, there are two opportunities to launch side-effects in a Redux pattern:
1. Reacting to a user-initiated action
2. Reacting to the result of a state change

The first will be covered in more detail below and is handled by middleware, where side-effects are started in reaction to some dispatched action.

The second requires a mechanism of observation for a Store's state, triggering some logic or work in response to state updates. On Android these observations are often started in the UI layer in order to tie them to lifecycle events. However, UI framework components like activities, fragments, and views are notoriously difficult to test and we want to avoid a situation where these classes become too large or complex.

There are already some existing mechanisms for doing this included in the lib state library:

1. `Fragment.consumeFrom` will setup a Store observation that is triggered on _every_ state update in the Store. The [HistoryFragment updating its view hierarchy with all state updates](https://github.com/mozilla-mobile/firefox-android/blob/910afa889ebc5222a1b9a877837de5c55244d441/fenix/app/src/main/java/org/mozilla/fenix/library/history/HistoryFragment.kt#L196) is one example.
2. `Flow.distinctUntilChanged` can be combined with things like `Fragment.consumeFlow` or `Store.flowScoped` for finer-grained state observations. This will allow reactions to specific state properties updates, like when the [HistorySearchDialogFragment changes the visibility of the AwesomeBar](https://github.com/mozilla-mobile/firefox-android/blob/910afa889ebc5222a1b9a877837de5c55244d441/fenix/app/src/main/java/org/mozilla/fenix/library/history/HistorySearchDialogFragment.kt#L189).

While both of these options are fine in many situations, they would both be difficult to setup to test. For a testable option, consider inheriting from [AbstractBinding](https://github.com/mozilla-mobile/firefox-android/blob/1cd69fec56725410af855eda923ab1afe99ae0b2/android-components/components/lib/state/src/main/java/mozilla/components/lib/state/helpers/AbstractBinding.kt#L23). This allows for defining a class that accepts dependencies and is isolated, making unit tests much easier. For some examples of that, see the [SelectedItemAdapterBinding](https://github.com/mozilla-mobile/firefox-android/blob/910afa889ebc5222a1b9a877837de5c55244d441/fenix/app/src/main/java/org/mozilla/fenix/tabstray/browser/BrowserTabsAdapter.kt#L61) or the [SwipeToDeleteBinding](https://github.com/mozilla-mobile/firefox-android/blob/910afa889ebc5222a1b9a877837de5c55244d441/fenix/app/src/main/java/org/mozilla/fenix/tabstray/browser/AbstractBrowserTrayList.kt#L35-L37) which are both used in the XML version of the Tabs Tray.

For more details, see the example refactor in the [example refactor section](#an-example-refactor-removing-interactors-and-controllers-from-historyfragment).

---

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

Here, we already see a view registering Store observers appropriately. The only thing that would need to change is the line calling the interactor, `onCategoryClick = interactor::onCategoryClicked,`. Following this line leads down a similar chain of abstraction layers, and eventually leads to `DefaultPocketStoriesController::handleCategoryClicked`. This function contains business logic which would need to be moved to the reducer and telemetry side-effects that would need to be moved to a middleware, but the ultimate intent is to toggle the selected category. We skip many layers of indirection by dispatching that action directly from the view:

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

---

### Extending the example: separating state and side-effects

To demonstrate the bullets above, here is the method definition in the `DefaultPocketStoriesController` that currently handles the business logic initiated from the `interactor::onCategoryClicked` call above.

```kotlin
    override fun handleCategoryClick(categoryClicked: PocketRecommendedStoriesCategory) {
        val initialCategoriesSelections = appStore.state.pocketStoriesCategoriesSelections

        // First check whether the category is clicked to be deselected.
        if (initialCategoriesSelections.map { it.name }.contains(categoryClicked.name)) {
            appStore.dispatch(AppAction.DeselectPocketStoriesCategory(categoryClicked.name))
            Pocket.homeRecsCategoryClicked.record(
                Pocket.HomeRecsCategoryClickedExtra(
                    categoryName = categoryClicked.name,
                    newState = "deselected",
                    selectedTotal = initialCategoriesSelections.size.toString(),
                ),
            )
            return
        }

        // If a new category is clicked to be selected:
        // Ensure the number of categories selected at a time is capped.
        val oldestCategoryToDeselect =
            if (initialCategoriesSelections.size == POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT) {
                initialCategoriesSelections.minByOrNull { it.selectionTimestamp }
            } else {
                null
            }
        oldestCategoryToDeselect?.let {
            appStore.dispatch(AppAction.DeselectPocketStoriesCategory(it.name))
        }

        // Finally update the selection.
        appStore.dispatch(AppAction.SelectPocketStoriesCategory(categoryClicked.name))

        Pocket.homeRecsCategoryClicked.record(
            Pocket.HomeRecsCategoryClickedExtra(
                categoryName = categoryClicked.name,
                newState = "selected",
                selectedTotal = initialCategoriesSelections.size.toString(),
            ),
        )
    }
```

If we break this down into a pseudocode algorithm, we get the following steps:
1. Read the current state from the store
2. If the category is being deselected
    1. Dispatch an action to update the state
    2. Record a metric that the category was deselected
    3. Return early
3. If the number of categories selected would be over max
    1. Dispatch an action deselecting the oldest
4. Dispatch an action with the newly selected category
5. Record a metric that a category was selected

In order to transform this algorithm into an appropriate usage of the Redux pattern, we can separate this list into two parts: steps that have side-effects and steps which can be represented by pure state updates. In this case, only the steps that record metrics represent side-effects. This will mean that those steps should be moved into a middleware, and the rest can be moved into a reducer.

Here's how that might look:
```kotlin
// add a new AppAction
data class TogglePocketStoriesCategory(val categoryName: String) : AppAction()

// add a case to handle it in a middleware. For example, let's put the following in MetricsMiddleware:
private fun handleAction(action: AppAction) = when (action) {
    ...
    is AppAction.TogglePocketStoriesCategory -> {
        val currentCategoriesSelections = currentState.pocketStoriesCategoriesSelections.map { it.name }
        // check if the category is being deselected
        if (currentCategoriesSelections.contains(action.categoryName)) {
            Pocket.homeRecsCategoryClicked.record(
                Pocket.HomeRecsCategoryClickedExtra(
                    categoryName = action.categoryName,
                    newState = "deselected",
                    selectedTotal = currentCategoriesSelections.size.toString(),
                ),
            )
        } else {
            Pocket.homeRecsCategoryClicked.record(
                Pocket.HomeRecsCategoryClickedExtra(
                    categoryName = action.categoryName,
                    newState = "selected",
                    selectedTotal = currentCategoriesSelections.size.toString(),
                ),
            )
        }
    }
}


// finally, we can shift all the state updating logic into the reducer
fun reduce(state: AppState, action: AppAction): AppState = when (action) {
    ...
    is AppAction.TogglePocketStoriesCategory -> {
        val currentCategoriesSelections = state.pocketStoriesCategoriesSelections.map { it.name }
        val updatedCategoriesState = when {
            currentCategoriesSelections.contains(action.categoryName) -> {
                state.copy(
                    pocketStoriesCategoriesSelections = state.pocketStoriesCategoriesSelections.filterNot {
                        it.name == action.categoryName
                    },
                )
            }
            currentCategoriesSelections.size == POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT -> {
                state.copy(
                    pocketStoriesCategoriesSelections = state.pocketStoriesCategoriesSelections
                        .minusOldest()
                        .plus(
                            PocketRecommendedStoriesSelectedCategory(
                                name = action.categoryName,
                            )
                        ),
                )
            }
            else -> {
                state.copy(
                    pocketStoriesCategoriesSelections =
                    state.pocketStoriesCategoriesSelections +
                            PocketRecommendedStoriesSelectedCategory(action.categoryName)
                )
            }
        }
        // Selecting a category means the stories to be displayed needs to also be changed.
        updatedCategoriesState.copy(
            pocketStories = updatedCategoriesState.getFilteredStories(),
        )
    }
}
```

---

### Interacting with the storage layer through middlewares

What if a feature requires some async data for its initial state, or needs to write changes to disk? There are no restrictions on running impure methods in middleware, so we can respond to an action that would make changes to the storage layer from the middleware. A good example is [ContainerMiddleware](https://github.com/mozilla-mobile/firefox-android/blob/5d773ae7710265ab98e15efc43b1afb17ab2e404/android-components/components/feature/containers/src/main/java/mozilla/components/feature/containers/ContainerMiddleware.kt)

Note that the `InitAction` here subscribes to a `Flow` from the database, which will continue to collect updates from the storage layer and dispatch actions to change the state accordingly.


Overall, this should convey the following improvements
- All state updates are guaranteed to be pure and in a single component, allowing for easy testing and reproduction.
- All side-effects are logically grouped into middlewares, allowing for testing strategies specific to the type of side-effect.
- Several indirect abstraction layers are removed, minimizing mental model of code.

---

### An example refactor: removing interactors and controllers from HistoryFragment

A [small example refactor](https://github.com/mozilla-mobile/firefox-android/pull/2347) was done to demonstrate some of the concepts presented throughout the document by removing interactors and controllers from the `HistoryFragment` and package. This is not necessarily an idealized version of that area, but is instead intended to demonstrate the first steps in creating architectural consistency.

---

## Drawbacks

Redux-like patterns have a bit of a learning curve, and some problems can be more difficult to solve than others. For example, handling side-effects like navigation or loading state from disk. The benefits of streamlining our architecture should outweigh this, especially once demonstrative examples of solving these problems are common in the codebase.

Additionally, the current implementation of our `Store` requires that dispatches are handled on a separate thread specific to the Store. This may change in the future, but also introduces some particular drawbacks:
- it can be harder to conceptualize the concurrency model, since there are no synchronous actions.
- thread switching has a performance impact.
- testing is more cumbersome by requiring additional async methods.

---

## Proposal acceptance and future plans

To demonstrate some of the concepts discussed in this proposal in more detail, an example patch will be produced that shows a refactor of a prominent feature of the app - history.

Following that, should this proposal be accepted the following suggestions are made for adoption with a focus on increasing the team's familiarity and knowledge of the pattern:

1. Architecture documentation should be updated with guidelines matching this proposal.
1. A number (1-3) of refactors should be planned to help demonstrate the pattern, especially in regards to things like using middleware to handle side-effects, how to structure local vs global state, and how to propagate state and actions to Composables and XML views.
1. Where possible, new feature work should require refactoring to the established architecture before feature implementation.
1. Investigate further improvements to the architecture. For example, a "single Store" model that more closely follows Redux patterns.

### Consolidating Stores

Multiple stores allows us to  manage memory with less investment. For example, state for a Settings UI screen can be held in a `SettingsStore` which can be cleared from memory (garbage collected) when no longer in that UI context.

This will mean that we will continue to have "global" stores like the `AppStore` and the `BrowserStore` co-existing with "local" stores like the `HistoryFragmentStore`. This can introduce complexity in terms of determining the best place to add state, middlewares, and actions.

Moving into the future, we will investigate options for consolidating our stores. It should be relatively straightforward to at least combine our global stores. Investigation is also ongoing into whether we can create "scoped" stores that combine a local store with the global store such that the local portion of the store can still automatically fall out of scope and respect the constraints of developing on mobile.
