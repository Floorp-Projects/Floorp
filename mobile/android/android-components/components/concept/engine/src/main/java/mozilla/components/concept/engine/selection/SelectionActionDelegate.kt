/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.selection

/**
 * Generic delegate for handling the context menu that is shown when text is selected.
 */
interface SelectionActionDelegate {
    /**
     * Gets Strings representing all possible selection actions.
     *
     * @returns String IDs for each action that could possibly be shown in the context menu. This
     * array must include all actions, available or not, and must not change over the class lifetime.
     */
    fun getAllActions(): Array<String>

    /**
     * Checks if an action can be shown on a new selection context menu.
     *
     * @returns whether or not the the custom action with the id of [id] is currently available
     *  which may be informed by [selectedText].
     */
    fun isActionAvailable(id: String, selectedText: String): Boolean

    /**
     * Gets a title to be shown in the selection context menu.
     *
     * @returns the text that should be shown on the action.
     */
    fun getActionTitle(id: String): CharSequence?

    /**
     * Should perform the action with the id of [id].
     *
     * @returns [true] if the action was consumed.
     */
    fun performAction(id: String, selectedText: String): Boolean

    /**
     * Takes in a list of actions and sorts them.
     *
     * @returns the sorted list.
     */
    fun sortedActions(actions: Array<String>): Array<String>
}
