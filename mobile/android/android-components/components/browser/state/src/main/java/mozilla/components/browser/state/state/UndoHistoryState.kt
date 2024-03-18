/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.browser.state.state.recover.RecoverableTab

/**
 * State keeping track of removed tabs to allow "undo".
 *
 * Currently the undo history only saves the tabs from the last remove operation. This is so far
 * "good enough" since we also only show one undo snackbar for the last operation in the UI.
 *
 * @param tag A tag (usually a UUID) identifying this specific undo state. This tag can be used to
 * avoid removing/restoring the wrong state in a multi-threaded environment.
 * @param tabs List of previously removed tabs.
 * @param selectedTabId Id of the tab in [tabs] that was selected and should get reselected on restore.
 */
data class UndoHistoryState(
    val tag: String = "",
    val tabs: List<RecoverableTab> = emptyList(),
    val selectedTabId: String? = null,
)
