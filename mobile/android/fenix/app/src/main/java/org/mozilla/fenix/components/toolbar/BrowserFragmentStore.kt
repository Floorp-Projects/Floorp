/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

class BrowserFragmentStore(initialState: BrowserFragmentState) :
    Store<BrowserFragmentState, BrowserFragmentAction>(initialState, ::browserStateReducer)

/**
 * The state for the Browser Screen
 * @property quickActionSheetState: state of the quick action sheet
 */
data class BrowserFragmentState(
    val quickActionSheetState: QuickActionSheetState
) : State

/**
 * The state for the QuickActionSheet
 * @property readable Whether or not the current session can display a reader view
 * @property bookmarked Whether or not the current session is already bookmarked
 * @property readerActive Whether or not the current session is in reader mode
 * @property bounceNeeded Whether or not the quick action sheet should bounce
 */
data class QuickActionSheetState(
    val readable: Boolean,
    val bookmarked: Boolean,
    val readerActive: Boolean,
    val bounceNeeded: Boolean,
    val isAppLink: Boolean
) : State

sealed class BrowserFragmentAction : Action

/**
 * Actions to dispatch through the [QuickActionSheetStore] to modify [QuickActionSheetState] through the reducer.
 */
sealed class QuickActionSheetAction : BrowserFragmentAction() {
    data class BookmarkedStateChange(val bookmarked: Boolean) : QuickActionSheetAction()
    data class ReadableStateChange(val readable: Boolean) : QuickActionSheetAction()
    data class ReaderActiveStateChange(val active: Boolean) : QuickActionSheetAction()
    data class AppLinkStateChange(val isAppLink: Boolean) : QuickActionSheetAction()
    object BounceNeededChange : QuickActionSheetAction()
}

/**
 * Reducers for [BrowserFragmentStore].
 *
 * A top level reducer that receives the current [BrowserFragmentState] and an [Action] and then
 * delegates to the proper child
 */
private fun browserStateReducer(
    state: BrowserFragmentState,
    action: BrowserFragmentAction
): BrowserFragmentState {
    return when (action) {
        is QuickActionSheetAction -> {
            QuickActionSheetStateReducer.reduce(state, action)
        }
    }
}

/**
 * Reduces [QuickActionSheetAction]s to update [BrowserFragmentState].
 */
internal object QuickActionSheetStateReducer {
    fun reduce(state: BrowserFragmentState, action: QuickActionSheetAction): BrowserFragmentState {
        return when (action) {
            is QuickActionSheetAction.BookmarkedStateChange ->
                state.copy(quickActionSheetState = state.quickActionSheetState.copy(bookmarked = action.bookmarked))
            is QuickActionSheetAction.ReadableStateChange ->
                state.copy(quickActionSheetState = state.quickActionSheetState.copy(readable = action.readable))
            is QuickActionSheetAction.ReaderActiveStateChange ->
                state.copy(quickActionSheetState = state.quickActionSheetState.copy(readerActive = action.active))
            is QuickActionSheetAction.BounceNeededChange ->
                state.copy(quickActionSheetState = state.quickActionSheetState.copy(bounceNeeded = true))
            is QuickActionSheetAction.AppLinkStateChange -> {
                state.copy(quickActionSheetState = state.quickActionSheetState.copy(isAppLink = action.isAppLink))
            }
        }
    }
}
