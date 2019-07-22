/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [QuickActionSheetState] and applying [QuickActionSheetAction]s.
 */
class QuickActionSheetStore(initialState: QuickActionSheetState) :
    Store<QuickActionSheetState, QuickActionSheetAction>(initialState, ::quickActionSheetStateReducer)

/**
 * The state for the QuickActionSheet found in the Browser Fragment
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

/**
 * Actions to dispatch through the [QuickActionSheetStore] to modify [QuickActionSheetState] through the reducer.
 */
sealed class QuickActionSheetAction : Action {
    data class BookmarkedStateChange(val bookmarked: Boolean) : QuickActionSheetAction()
    data class ReadableStateChange(val readable: Boolean) : QuickActionSheetAction()
    data class ReaderActiveStateChange(val active: Boolean) : QuickActionSheetAction()
    data class AppLinkStateChange(val isAppLink: Boolean) : QuickActionSheetAction()
    object BounceNeededChange : QuickActionSheetAction()
}

/**
 * Reduces [QuickActionSheetAction]s to update [QuickActionSheetState].
 */
fun quickActionSheetStateReducer(
    state: QuickActionSheetState,
    action: QuickActionSheetAction
): QuickActionSheetState {
    return when (action) {
        is QuickActionSheetAction.BookmarkedStateChange ->
            state.copy(bookmarked = action.bookmarked)
        is QuickActionSheetAction.ReadableStateChange ->
            state.copy(readable = action.readable)
        is QuickActionSheetAction.ReaderActiveStateChange ->
            state.copy(readerActive = action.active)
        is QuickActionSheetAction.BounceNeededChange ->
            state.copy(bounceNeeded = true)
        is QuickActionSheetAction.AppLinkStateChange -> {
            state.copy(isAppLink = action.isAppLink)
        }
    }
}
