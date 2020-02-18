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

private val customActions = arrayOf(SEARCH, SEARCH_PRIVATELY)

/**
 * Adds normal and private search buttons to text selection context menus.
 */
class DefaultSelectionActionDelegate(
    private val searchAdapter: SearchAdapter,
    resources: Resources,
    appName: String
) : SelectionActionDelegate {

    private val normalSearchText = resources.getString(R.string.mozac_selection_context_menu_search, appName)
    private val privateSearchText = resources.getString(R.string.mozac_selection_context_menu_search_privately, appName)

    override fun getAllActions(): Array<String> = customActions

    override fun isActionAvailable(id: String): Boolean {
        val isPrivate = searchAdapter.isPrivateSession()
        return (id == SEARCH && !isPrivate) ||
            (id == SEARCH_PRIVATELY && isPrivate)
    }

    override fun getActionTitle(id: String): CharSequence? = when (id) {
        SEARCH -> normalSearchText
        SEARCH_PRIVATELY -> privateSearchText
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
        else -> false
    }
}
