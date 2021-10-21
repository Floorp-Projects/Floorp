---
layout: page
title: Adding tab partitions and groups to BrowserState
permalink: /rfc/0008-tab-groups
---

* Start date: 2021-10-25
* RFC PR: [#11172](https://github.com/mozilla-mobile/android-components/pull/11172)

## Summary

Fenix has recently introduced automatic tab groups, as well as a new section for inactive tabs. Both concepts are essentially partitioning tabs for display purposes. This RFC describes a proposal to move these tab partitions and groups to our `BrowserState` so they are stored and managed in the same place as tabs.

## Motivation

Currently consumers of Android Components have to manage their own state for tab groups, which has the following disadvantages:

* Redundant code needs to be written and maintained for managing any grouping of tabs.

* As the tab groups are detached from the `BrowserState`, this code is also harder to write e.g., consuming applications have to take care of keeping the group state in sync with tab state in the `BrowserStore`.

* Consuming applications need to persist tab groups and their state separately from the existing tab/session storage, and keep these storages in sync. Note that Fenix doesn't need to do this yet, as groups are inferred based on tab state that is already persisted as part of the Android Component storage, but it's easy to see future use cases where this will be required e.g., user-managed groups.

* Functionality in Android Components can interfere with partitioning and grouping in applications. Android components currently have no knowledge of how the consuming application decides to group tabs, which leads to feature gaps and bugs that cannot be addressed (or only with suboptimal workarounds.) One example of this is the overlap between inactive tabs and the tab selection logic in our `BrowserState` reducer. Our reducer logic makes sure that there's always a selected tab, so when a tab is closed, another one will be selected. However, in Fenix, when the last active tab is closed, we don't want to select an inactive tab [1]. This is especially true as selecting an inactive tab will cause the `LastAccessMiddleware` in Android Components to update the `lastAccess` time of this tab, which will incorrectly mark it as active again. While a solution to this problem will always involve consuming code i.e., only the application really understands how grouping should work, it becomes much easier if Android Components has a concept of groups. A feature could then be configured not to apply to tabs in a specific group for instance.

[1] [https://github.com/mozilla-mobile/fenix/issues/21785](https://github.com/mozilla-mobile/fenix/issues/21785)

## Guide-level explanation

The proposal is to add a data structure to the existing `BrowserState` that is capable of representing generic tab groups and also allows for preventing undesired overlap of groups targeting independent features. Identifying groups by name isn't sufficient, as an application could use the same name for groups targeting different concepts e.g., an "inactive" group could be used for the inactive tab feature, but "inactive" could also be a search term group. The data structure and APIs we choose should make it hard to get this wrong and always require consuming applications to explicitly specify the "type" of group being targeted.

To separate these different types of groups, and to prevent a group of groups concept, the proposal is to introduce the concept of a tab *partition*. A partition has a unique name and holds groups for a specific feature e.g., the current search term groups in Fenix would be contained in a partition named `AUTOMATIC_SEARCH_TERM_GROUPS` or similar, with the actual tab groups nested inside this partition grouped by search term.

In addition to the data structure, we want to provide a set of actions to manipulate (create, update, delete) groups, and to add/remove tabs from groups. It should be easy to implement both automatic grouping mechanisms, as well as manual ones carried out by the user. These actions can be triggered by middlewares for automatic grouping and via `UseCases` when carried out by the user, matching our current architecture.

## Reference-level explanation

We will add tab partitions and groups to our browser state without affecting the existing data structures. 

### State

`BrowserState` will get a new `tabPartitions` property typed as `Map<String, TabPartition>`, mapping the partition ID to a newly introduced type called `TabPartition`. Partitions are used to associate tab groups with a specific feature or use case:

```Kotlin
data class BrowserState(
   val tabs: List<TabSessionState> = emptyList(), // Remains unchanged
   val tabPartitions: Map<String, TabPartition> = emptyMap(), // Newly added
   ..
)

/**
 * Value type representing a tab partition. Partitions can overlap i.e., a tab
 * can be in multiple partitions at the same time.
 *
 * @property id The ID of a tab partition. This should uniquely identify
 * the feature responsible for managing those groups.
 * @property tabGroups The groups of tabs in this partition. A partition can
 * have one or more groups, depending on use case. Empty partitions will be
 * removed by the system.
 */
data class TabPartition(
    val id: String,
    val tabGroups: List<TabGroup>
)

/**
 * Value type representing a tab group.
 *
 * @property id The unique ID of this tab group, default to a generated UUID.
 * @property name The name of this tab group for display purposes.
 * @property persist Whether or not this tab group should be persisted, defaults to false.
 * @property tabIds The IDs of all tabs in this group, defaults to empty list.
 */
data class TabGroup(
    val id: String = UUID.randomUUID().toString(),
    val name: String = "",
    val persist: Boolean = false,
    val tabIds: List<String> = emptyList()
)
```

So, a `TabPartition` is used to relate tab groups to a specific feature, thereby structurally separating it from groups of other features. They are not disjoint i.e., we will allow overlapping partitions. As an example, a tab could be in an "inactive" group, but also retain a group association provided by the user. So if this tab was opened and became active again, it would rejoin its original group. A partition could have one or more groups e.g. for a feature like inactive tabs we could use a single `default` group holding just inactive tabs, or two groups for `active` and `inactive` tabs. This ultimately depends on the use case and how the groups are supposed to be displayed.

A `TabGroup` contains a list of tabs. We only store the tab IDs, not for performance reasons, but because most of our `TabActions`s are based on IDs and this makes it much easier to use, compared to having to look up the `TabSessionState` every time. We will provide extension functions on `TabGroup` to accept and return `TabSessionState` objects so that consumers can easily look up (or add/remove) the actual tab objects in the group. Note that we're using a `List` (as opposed to a `Set`) to support custom ordering (see [Future Work](#future-work)). A `TabGroup` further has a unique ID and name for display purposes. The `persist` flag can be used to configure whether or not a group should be persisted and restored after an app restart.

We will also provide extension functions to look up tab groups by name or ID via its partition:

```Kotlin
/**
 * Returns the first tab group matching the provided [name]. Note that we allow
 * multiple groups with the same name in a partition but disambiguation needs
 * to be handled on a feature level.
 */
fun TabPartition.getGroupByName(name: String) = this.tabGroups.first {
    it.name.equals(name, ignoreCase = true)
}

/**
 * Returns the tab groups for the provided [id].
 */
fun TabPartition.getGroupById(id: String) = this.tabGroups.first {
    it.id == id
}
```

Here's an example state for illustration purposes. There will be no need to ever write it out this way, except maybe in unit tests:

```Kotlin
BrowserState(
    tabPartitions = mapOf(
        AUTOMATIC_SEARCH_TERM_GROUPS to TabPartition(
            id = AUTOMATIC_SEARCH_TERM_GROUPS,
            tabGroups = listOf(
                TabGroup(
                    name = "running shoes",
                    tabIds = listOf(...)
                ),
                TabGroup(
                    name = "toronto population",
                    tabIds = listOf(...)
                )
            )
        ),
        INACTIVE_TABS to TabPartition(
            id = INACTIVE_TABS,
            tabGroups = listOf(
                TabGroup(
                    name = "default",
                    tabIds = listOf(...)
                )
            )
        )
    )
)
```


### Actions

We will add the following actions to manage partitions and groups. This isn't necessarily a complete list but should illustrate the idea:

```Kotlin
/**
 * [BrowserAction] implementations related to updating tab partitions and groups inside [BrowserState].
 */
sealed class TabGroupAction : BrowserAction() {

    /**
     * Adds a new group to [BrowserState.tabPartitions]. If the corresponding partition
     * doesn't exist it will be created.
     *
     * @property partition the ID of the partition the group belongs to.
     * @property group the [TabGroup] to add.
     */
    data class AddTabGroupAction(
        val partition: String,
        val group: TabGroup
    ) : TabGroupAction()

    /**
     * Removes a group from [BrowserState.tabPartitions]. Note that empty partitions
     * will be removed.
     *
     * @property partition the ID of the partition the group belongs to.
     * @property group the ID of the group to remove.
     */
    data class RemoveTabGroupAction(
        val partition: String,
        val group: String
    ) : TabGroupAction()

    /**
     * Adds the provided tab to a group in [BrowserState].
     *
     * @property partition the ID of the partition the group belongs to. If the corresponding
     * partition doesn't exist it will be created.
     * @property group the ID of the group.
     * @property tabId the ID of the tab to add to the group.
     */
    data class AddTabAction(
        val partition: String,
        val group: String,
        val tabId: String,
    ) : TabGroupAction()

    /**
     * Adds the provided tabs to a group in [BrowserState].
     *
     * @property partition the ID of the partition the group belongs to. If the corresponding
     * partition doesn't exist it will be created.
     * @property group the ID of the group.
     * @property tabIds the IDs of the tabs to add to the group.
     */
    data class AddTabsAction(
        val partition: String,
        val group: String,
        val tabIds: List<String>
    ) : TabGroupAction()

    /**
     * Removes the provided tab from a group in [BrowserState].
     *
     * @property partition the ID of the partition the group belongs to. If the corresponding
     * partition doesn't exist it will be created.
     * @property group the ID of the group.
     * @property tabId the ID of the tab to remove from the group.
     */
    data class RemoveTabAction(
        val partition: String,
        val group: String,
        val tabId: String
    ) : TabGroupAction()

    /**
     * Removes the provided tabs from a group in [BrowserState].
     *
     * @property partition the ID of the partition the group belongs to. If the corresponding
     * partition doesn't exist it will be created.
     * @property group the ID of the group.
     * @property tabIds the IDs of the tabs to remove from the group.
     */
    data class RemoveTabsAction(
        val partition: String,
        val group: String,
        val tabIds: List<String>
    ) : TabGroupAction()
}
```

The purpose of these actions is described in the KDocs above, but should not deviate from our current way of doing things. See next chapters for more details.

### Automatic grouping with middlewares

Middlewares can be used to implement automatic tab grouping functionality, as they can react to state changes in the system and dispatch the corresponding group actions. Here is an example of how automatic search term grouping could be implemented today using the actions described above:

```Kotlin
/**
 * This [Middleware] manages tab groups for search terms.
 */
class SearchTermTabGroupMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        when (action) {
            is HistoryMetadataAction.SetHistoryMetadataKeyAction -> {
                action.historyMetadataKey.searchTerm?.let { searchTerms ->
                    context.dispatch(TabGroupAction.AddTabAction(SEARCH_TERM_GROUPS, searchTerms, action.tabId))
                }
            }
            is HistoryMetadataAction.DisbandSearchGroupAction -> {
                val group = context.state.tabPartitions[SEARCH_TERM_GROUPS]?.getGroupByName(action.searchTerm)
                group?.let {
                    context.dispatch(TabGroupAction.RemoveTabGroupAction(SEARCH_TERM_GROUPS, it.id))
                }
            }
        }

        next(action)
    }

```

This middleware automatically adds and removes tabs once they're associated/disassociated with search terms based on history metadata. `AUTOMATIC_SEARCH_TERM_GROUPS` is the ID of the partition, the search terms (e.g. "Toronto") are used as group names. 

Using middlewares provides all the flexibility we could need e.g., a middleware can put tabs in one or multiple groups, and generally is capable of observing all state changes in the system for grouping/ungrouping purposes.

### Manual grouping with UseCases

I am not adding a specific code example here, as this is following our existing architecture e.g., `TabsUseCases` dispatches a `TabListAction.RemoveTabAction(tabId)` when the `removeTab` use case is invoked. We would provide a set of similar use cases that in turn dispatch the corresponding actions described above. So, `TabGroupUseCases` would dispatch a `RemoveTabAction(partition, group, tabId)` when the corresponding `removeTab` use case is invoked. These `UseCases` can be used by multiple features to implement manual grouping mechanisms. This will also allow for implementing overlapping use cases between automatic and manual groups e.g., a tab could be automatically grouped, but moved to another group by the user.

### Reducers

We will need to introduce a new `Reducer` for the tab group actions outlined above. We will also need to either update our existing `TabListReducer` or handle `TabListAction`s in our new `TabGroupReducer`to make sure we disassociate tabs from their groups when they get deleted. Keeping this state in sync is not something we should let every single middleware or feature to deal with itself. It's relatively easy to handle this as part of our reducers.

### Persistence

We should also support persisting tab partitions and groups using our `BrowserStateWriter` and `BrowserStateReader`. We currently plan to make this configurable on a group level. For some groups it may make more sense to recreate them in memory as needed e.g., for search term groups we already store the required metadata as part of tabs so it would be easy to recreate the groups on startup without having to persist additional data. The same is true for inactive tabs.


## Alternatives

* We could consider flattening this data structure. So, instead of the nested Partition/Group structure, we could use a single `Map<GroupKey, List<String>>` where the `GroupKey` contains both the name of the group, as well as a `type` or `feature` identifier. This would make the actions somewhat easier, as we could remove the `partition` parameter, but it would also be easier to make mistakes. An application could more easily look up or provide the wrong group, and it feels a bit unorganized :).


## Future Work

* We have discussed separating private tabs into their own collection in `BrowserState`. This work could also be combined with the proposal here to have normal and private tab groups within a partition. Alternatively, we could also introduce new top level types for `PrivateTabs` and `NormalTabs` with the corresponding parititions nested inside. Then the data structure described here wouldn't need to change. This discussion and refactoring may be best done separately though.

* We have recently introduced actions to move / re-order tabs [2]. Today these actions can not be used in combination with tab groups. The proposal here would allow for it though, as we could introduce versions of these actions that apply to partitions/groups.

[2] [https://github.com/mozilla-mobile/android-components/pull/10936/](https://github.com/mozilla-mobile/android-components/pull/10936/)


## Questions

* Is this design too simple? Does it cover all use cases we currently anticipate?
	* We worked through this in this RFC and decided that this would cover our current use cases as far as we know. One case that is not covered with this is supporting an ordered but mixed display of tabs and groups. We have no way of positining a group withing a list of tabs and vice versa, but this is outside the scope of this RFC.
* Do we need to implement persistence now or should we punt on this and handle in a later iteration?
	* We decided to make persistence configurable on a group level. The actual implementation of this can land in multiple iterations as we currently don't need to persist groups.
