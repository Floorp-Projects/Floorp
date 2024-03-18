/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.concept.storage.Login
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.utils.Settings

/**
 * Class representing a parcelable saved logins item
 * @property guid The id of the saved login
 * @property origin Site of the saved login
 * @property username Username that's saved for this site
 * @property password Password that's saved for this site
 * @property timeLastUsed Time of last use in milliseconds from the unix epoch.
 */
@Parcelize
data class SavedLogin(
    val guid: String,
    val origin: String,
    val username: String,
    val password: String,
    val timeLastUsed: Long,
) : Parcelable

fun Login.mapToSavedLogin(): SavedLogin =
    SavedLogin(
        guid = this.guid,
        origin = this.origin,
        username = this.username,
        password = this.password,
        timeLastUsed = this.timeLastUsed,
    )

/**
 * The [Store] for holding the [LoginsListState] and applying [LoginsAction]s.
 */
class LoginsFragmentStore(initialState: LoginsListState) :
    Store<LoginsListState, LoginsAction>(
        initialState,
        ::savedLoginsStateReducer,
    )

/**
 * Actions to dispatch through the `LoginsFragmentStore` to modify `LoginsListState` through the reducer.
 */
sealed class LoginsAction : Action {
    data class FilterLogins(val newText: String?) : LoginsAction()
    data class UpdateLoginsList(val list: List<SavedLogin>) : LoginsAction()
    data class AddLogin(val newLogin: SavedLogin) : LoginsAction()
    data class UpdateLogin(val loginId: String, val newLogin: SavedLogin) : LoginsAction()
    data class DeleteLogin(val loginId: String) : LoginsAction()
    object LoginsListUpToDate : LoginsAction()
    data class UpdateCurrentLogin(val item: SavedLogin) : LoginsAction()
    data class SortLogins(val sortingStrategy: SortingStrategy) : LoginsAction()
    data class DuplicateLogin(val dupe: SavedLogin?) : LoginsAction()
    data class LoginSelected(val item: SavedLogin) : LoginsAction()
}

/**
 * The state for the Saved Logins Screen.
 *
 * @property isLoading Whether or not the list of logins are being loaded.
 * @property loginList Filterable list of [SavedLogin]s that persist in storage.
 * @property filteredItems Filtered list of [SavedLogin]s to display.
 * @property currentItem The last item that was opened in the detail view.
 * @property searchedForText String used by the user to filter logins.
 * @property sortingStrategy Sorting strategy selected by the user. Currently, we support
 * sorting alphabetically and by last used.
 * @property highlightedItem The current selected sorting strategy from the sort menu.
 * @property duplicateLogin Duplicate login for the current add/save login form.
 */
data class LoginsListState(
    val isLoading: Boolean = false,
    val loginList: List<SavedLogin>,
    val filteredItems: List<SavedLogin>,
    val currentItem: SavedLogin? = null,
    val searchedForText: String?,
    val sortingStrategy: SortingStrategy,
    val highlightedItem: SavedLoginsSortingStrategyMenu.Item,
    val duplicateLogin: SavedLogin? = null,
) : State

fun createInitialLoginsListState(settings: Settings) = LoginsListState(
    isLoading = true,
    loginList = emptyList(),
    filteredItems = emptyList(),
    searchedForText = null,
    sortingStrategy = settings.savedLoginsSortingStrategy,
    highlightedItem = settings.savedLoginsMenuHighlightedItem,
)

/**
 * Handles changes in the saved logins list, including updates and filtering.
 */
private fun savedLoginsStateReducer(
    state: LoginsListState,
    action: LoginsAction,
): LoginsListState {
    return when (action) {
        is LoginsAction.LoginsListUpToDate -> {
            state.copy(isLoading = false)
        }
        is LoginsAction.UpdateLoginsList -> {
            state.copy(
                isLoading = false,
                loginList = action.list,
                filteredItems = state.sortingStrategy(action.list),
            )
        }
        is LoginsAction.AddLogin -> {
            val updatedLogins = state.loginList + action.newLogin
            state.copy(
                isLoading = false,
                loginList = updatedLogins,
                filteredItems = state.sortingStrategy(updatedLogins),
            )
        }
        is LoginsAction.UpdateLogin -> {
            val updatedLogins = state.loginList.map {
                when (it.guid == action.loginId) {
                    true -> action.newLogin
                    false -> it
                }
            }
            state.copy(
                isLoading = false,
                loginList = updatedLogins,
                filteredItems = state.sortingStrategy(updatedLogins),
            )
        }
        is LoginsAction.DeleteLogin -> {
            val updatedLogins = state.loginList.filterNot { it.guid == action.loginId }
            state.copy(
                loginList = updatedLogins,
                filteredItems = state.sortingStrategy(updatedLogins),
            )
        }
        is LoginsAction.FilterLogins -> {
            filterItems(
                action.newText,
                state.sortingStrategy,
                state,
            )
        }
        is LoginsAction.UpdateCurrentLogin -> {
            state.copy(
                currentItem = action.item,
            )
        }
        is LoginsAction.SortLogins -> {
            filterItems(
                state.searchedForText,
                action.sortingStrategy,
                state,
            )
        }
        is LoginsAction.LoginSelected -> {
            state.copy(
                isLoading = true,
            )
        }
        is LoginsAction.DuplicateLogin -> {
            state.copy(
                duplicateLogin = action.dupe,
            )
        }
    }
}

/**
 * @return [LoginsListState] containing a new [LoginsListState.filteredItems]
 * with filtered [LoginsListState.items]
 *
 * @param searchedForText based on which [LoginsListState.items] will be filtered.
 * @param sortingStrategy based on which [LoginsListState.items] will be sorted.
 * @param state previous [LoginsListState] containing all the other properties
 * with which a new state will be created
 */
private fun filterItems(
    searchedForText: String?,
    sortingStrategy: SortingStrategy,
    state: LoginsListState,
): LoginsListState {
    return if (searchedForText.isNullOrBlank()) {
        state.copy(
            isLoading = false,
            sortingStrategy = sortingStrategy,
            highlightedItem = sortingStrategyToMenuItem(sortingStrategy),
            searchedForText = searchedForText,
            filteredItems = sortingStrategy(state.loginList),
        )
    } else {
        state.copy(
            isLoading = false,
            sortingStrategy = sortingStrategy,
            highlightedItem = sortingStrategyToMenuItem(sortingStrategy),
            searchedForText = searchedForText,
            filteredItems = sortingStrategy(state.loginList).filter {
                it.origin.contains(
                    searchedForText,
                    true,
                ) || it.username.contains(
                    searchedForText,
                    true,
                )
            },
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
