/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.res.Resources
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.concept.engine.selection.SelectionActionDelegate

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SEARCH = "CUSTOM_CONTEXT_MENU_SEARCH"
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SEARCH_PRIVATELY = "CUSTOM_CONTEXT_MENU_SEARCH_PRIVATELY"
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SHARE = "CUSTOM_CONTEXT_MENU_SHARE"

private val customActions = arrayOf(SEARCH, SEARCH_PRIVATELY, SHARE)

/**
 * Adds normal and private search buttons to text selection context menus.
 */
class DefaultSelectionActionDelegate(
    private val searchAdapter: SearchAdapter,
    resources: Resources,
    private val shareTextClicked: ((String) -> Unit)? = null,
    private val actionSorter: ((Array<String>) -> Array<String>)? = null
) : SelectionActionDelegate {

    private val normalSearchText =
        resources.getString(R.string.mozac_selection_context_menu_search_2)
    private val privateSearchText =
        resources.getString(R.string.mozac_selection_context_menu_search_privately_2)
    private val shareText = resources.getString(R.string.mozac_selection_context_menu_share)

    override fun getAllActions(): Array<String> = customActions

    override fun isActionAvailable(id: String): Boolean {
        val isPrivate = searchAdapter.isPrivateSession()
        return (id == SHARE && shareTextClicked != null) ||
                (id == SEARCH && !isPrivate) ||
                (id == SEARCH_PRIVATELY && isPrivate)
    }

    override fun getActionTitle(id: String): CharSequence? = when (id) {
        SEARCH -> normalSearchText
        SEARCH_PRIVATELY -> privateSearchText
        SHARE -> shareText
        else -> null
    }

    override fun performAction(id: String, selectedText: String): Boolean = when (id) {
        SEARCH -> {
            searchAdapter.sendSearch(false, selectedText)
            true
        }
        SEARCH_PRIVATELY -> {
            searchAdapter.sendSearch(true, selectedText)
            true
        }
        SHARE -> {
            shareTextClicked?.invoke(selectedText)
            true
        }
        else -> false
    }

    /**
     * Takes in a list of actions and sorts them.
     * @returns the sorted list.
     */
    override fun sortedActions(actions: Array<String>): Array<String> {
        return if (actionSorter != null) {
            actionSorter.invoke(actions)
        } else {
            actions
        }
    }
}
