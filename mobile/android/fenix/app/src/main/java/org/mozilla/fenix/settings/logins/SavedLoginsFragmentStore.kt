/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing an saved logins item
 * @property url Site of the saved login
 * @property userName Username that's saved for this site
 * @property password Password that's saved for this site
 * @property id The unique identifier for this login entry
 * @property timeLastUsed Time of last use in milliseconds from the unix epoch.
 */
@Parcelize
data class SavedLoginsItem(
    val url: String,
    val userName: String?,
    val password: String?,
    val id: String,
    val timeLastUsed: Long
) :
    Parcelable

/**
 * The [Store] for holding the [SavedLoginsFragmentState] and applying [SavedLoginsFragmentAction]s.
 */
class SavedLoginsFragmentStore(initialState: SavedLoginsFragmentState) :
    Store<SavedLoginsFragmentState, SavedLoginsFragmentAction>(
        initialState,
        ::savedLoginsStateReducer
    )

/**
 * Actions to dispatch through the `SavedLoginsStore` to modify `SavedLoginsFragmentState` through the reducer.
 */
sealed class SavedLoginsFragmentAction : Action {
    data class FilterLogins(val newText: String?) : SavedLoginsFragmentAction()
    data class UpdateLogins(val list: List<SavedLoginsItem>) : SavedLoginsFragmentAction()
    data class SortLogins(val sortingStrategy: SortingStrategy) : SavedLoginsFragmentAction()
}

/**
 * The state for the Saved Logins Screen
 * @property isLoading State to know when to show loading
 * @property items Source of truth of list of logins
 * @property filteredItems Filtered (or not) list of logins to display
 * @property searchedForText String used by the user to filter logins
 * @property sortingStrategy sorting strategy selected by the user (Currently we support
 * sorting alphabetically and by last used)
 */
data class SavedLoginsFragmentState(
    val isLoading: Boolean = false,
    val items: List<SavedLoginsItem>,
    val filteredItems: List<SavedLoginsItem>,
    val searchedForText: String?,
    val sortingStrategy: SortingStrategy,
    val highlightedItem: SavedLoginsSortingStrategyMenu.Item
) : State

/**
 * The SavedLoginsState Reducer.
 */
private fun savedLoginsStateReducer(
    state: SavedLoginsFragmentState,
    action: SavedLoginsFragmentAction
): SavedLoginsFragmentState {
    return when (action) {
        is SavedLoginsFragmentAction.UpdateLogins -> {
            filterItems(
                state.searchedForText, state.sortingStrategy, state.copy(
                    isLoading = false,
                    items = action.list,
                    filteredItems = emptyList()
                )
            )
        }
        is SavedLoginsFragmentAction.FilterLogins ->
            filterItems(
                action.newText,
                state.sortingStrategy,
                state
            )
        is SavedLoginsFragmentAction.SortLogins ->
            filterItems(
                state.searchedForText,
                action.sortingStrategy,
                state
            )
    }
}

/**
 * @return [SavedLoginsFragmentState] containing a new [SavedLoginsFragmentState.filteredItems]
 * with filtered [SavedLoginsFragmentState.items]
 *
 * @param searchedForText based on which [SavedLoginsFragmentState.items] will be filtered.
 * @param sortingStrategy based on which [SavedLoginsFragmentState.items] will be sorted.
 * @param state previous [SavedLoginsFragmentState] containing all the other properties
 * with which a new state will be created
 */
private fun filterItems(
    searchedForText: String?,
    sortingStrategy: SortingStrategy,
    state: SavedLoginsFragmentState
): SavedLoginsFragmentState {
    return if (searchedForText.isNullOrBlank()) {
        state.copy(
            isLoading = false,
            sortingStrategy = sortingStrategy,
            highlightedItem = sortingStrategyToMenuItem(sortingStrategy),
            searchedForText = searchedForText,
            filteredItems = sortingStrategy(state.items)
        )
    } else {
        state.copy(
            isLoading = false,
            sortingStrategy = sortingStrategy,
            highlightedItem = sortingStrategyToMenuItem(sortingStrategy),
            searchedForText = searchedForText,
            filteredItems = sortingStrategy(state.items).filter {
                it.url.contains(
                    searchedForText
                )
            }
        )
    }
}

private fun sortingStrategyToMenuItem(sortingStrategy: SortingStrategy): SavedLoginsSortingStrategyMenu.Item {
    return when (sortingStrategy) {
        is SortingStrategy.Alphabetically -> {
            SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort
        }

        is SortingStrategy.LastUsed -> {
            SavedLoginsSortingStrategyMenu.Item.LastUsedSort
        }
    }
}
