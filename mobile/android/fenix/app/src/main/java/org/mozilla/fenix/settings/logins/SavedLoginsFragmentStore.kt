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
 */
@Parcelize
data class SavedLoginsItem(val url: String, val userName: String?, val password: String?) :
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
    data class UpdateLogins(val list: List<SavedLoginsItem>) : SavedLoginsFragmentAction()
}

/**
 * The state for the Saved Logins Screen
 * @property items List of logins to display
 */
data class SavedLoginsFragmentState(val items: List<SavedLoginsItem>) : State

/**
 * The SavedLoginsState Reducer.
 */
private fun savedLoginsStateReducer(
    state: SavedLoginsFragmentState,
    action: SavedLoginsFragmentAction
): SavedLoginsFragmentState {
    return when (action) {
        is SavedLoginsFragmentAction.UpdateLogins -> state.copy(items = action.list)
    }
}
