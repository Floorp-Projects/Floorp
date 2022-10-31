/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.res.Resources
import android.util.Patterns
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.feature.contextmenu.facts.emitTextSelectionClickFact
import mozilla.components.feature.search.SearchAdapter

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SEARCH = "CUSTOM_CONTEXT_MENU_SEARCH"

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SEARCH_PRIVATELY = "CUSTOM_CONTEXT_MENU_SEARCH_PRIVATELY"

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val SHARE = "CUSTOM_CONTEXT_MENU_SHARE"

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val EMAIL = "CUSTOM_CONTEXT_MENU_EMAIL"

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val CALL = "CUSTOM_CONTEXT_MENU_CALL"

private val customActions = arrayOf(CALL, EMAIL, SEARCH, SEARCH_PRIVATELY, SHARE)

/**
 * Adds normal and private search buttons to text selection context menus.
 * Also adds share, email, and call actions which are optionally displayed.
 */
@Suppress("LongParameterList")
class DefaultSelectionActionDelegate(
    private val searchAdapter: SearchAdapter,
    resources: Resources,
    private val shareTextClicked: ((String) -> Unit)? = null,
    private val emailTextClicked: ((String) -> Unit)? = null,
    private val callTextClicked: ((String) -> Unit)? = null,
    private val actionSorter: ((Array<String>) -> Array<String>)? = null,
) : SelectionActionDelegate {

    private val normalSearchText =
        resources.getString(R.string.mozac_selection_context_menu_search_2)
    private val privateSearchText =
        resources.getString(R.string.mozac_selection_context_menu_search_privately_2)
    private val shareText = resources.getString(R.string.mozac_selection_context_menu_share)
    private val emailText = resources.getString(R.string.mozac_selection_context_menu_email)
    private val callText = resources.getString(R.string.mozac_selection_context_menu_call)

    override fun getAllActions(): Array<String> = customActions

    @SuppressWarnings("ComplexMethod")
    override fun isActionAvailable(id: String, selectedText: String): Boolean {
        val isPrivate = searchAdapter.isPrivateSession()
        return (id == SHARE && shareTextClicked != null) ||
            (
                id == EMAIL && emailTextClicked != null &&
                    Patterns.EMAIL_ADDRESS.matcher(selectedText.trim()).matches()
                ) ||
            (
                id == CALL &&
                    callTextClicked != null && Patterns.PHONE.matcher(selectedText.trim()).matches()
                ) ||
            (id == SEARCH && !isPrivate) ||
            (id == SEARCH_PRIVATELY && isPrivate)
    }

    override fun getActionTitle(id: String): CharSequence? = when (id) {
        SEARCH -> normalSearchText
        SEARCH_PRIVATELY -> privateSearchText
        SHARE -> shareText
        EMAIL -> emailText
        CALL -> callText
        else -> null
    }

    override fun performAction(id: String, selectedText: String): Boolean {
        emitTextSelectionClickFact(id)
        return when (id) {
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
            EMAIL -> {
                emailTextClicked?.invoke(selectedText.trim())
                true
            }
            CALL -> {
                callTextClicked?.invoke(selectedText.trim())
                true
            }
            else -> {
                false
            }
        }
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
