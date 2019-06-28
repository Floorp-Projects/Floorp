/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Action

/**
 * [Action] implementation related to [BrowserState].
 */
sealed class BrowserAction : Action
/**
 * [BrowserAction] implementations related to updating the list of [TabSessionState] inside [BrowserState].
 */
sealed class TabListAction : BrowserAction() {
    /**
     * Adds a new [TabSessionState] to the list.
     */
    data class AddTabAction(val tab: TabSessionState, val select: Boolean = false) : TabListAction()

    /**
     * Marks the [TabSessionState] with the given [tabId] as selected tab.
     */
    data class SelectTabAction(val tabId: String) : TabListAction()

    /**
     * Removes the [TabSessionState] with the given [tabId] from the list of sessions.
     */
    data class RemoveTabAction(val tabId: String) : TabListAction()

    /**
     * Restores state from a (partial) previous state.
     */
    data class RestoreAction(val tabs: List<TabSessionState>, val selectedTabId: String? = null) : TabListAction()

    /**
     * Removes both private and normal [TabSessionState]s.
     */
    object RemoveAllTabsAction : TabListAction()

    /**
     * Removes all private [TabSessionState]s.
     */
    object RemoveAllPrivateTabsAction : TabListAction()

    /**
     * Removes all non-private [TabSessionState]s.
     */
    object RemoveAllNormalTabsAction : TabListAction()
}

/**
 * [BrowserAction] implementations related to updating [BrowserState.customTabs].
 */
sealed class CustomTabListAction : BrowserAction() {
    /**
     * Adds a new [CustomTabSessionState] to [BrowserState.customTabs].
     */
    data class AddCustomTabAction(val tab: CustomTabSessionState) : CustomTabListAction()

    /**
     * Removes an existing [CustomTabSessionState] to [BrowserState.customTabs].
     */
    data class RemoveCustomTabAction(val tabId: String) : CustomTabListAction()

    /**
     * Removes all custom tabs [TabSessionState]s.
     */
    object RemoveAllCustomTabsAction : CustomTabListAction()
}

/**
 * [BrowserAction] implementations related to updating the [ContentState] of a single [SessionState] inside
 * [BrowserState].
 */
sealed class ContentAction : BrowserAction() {
    /**
     * Updates the URL of the [ContentState] with the given [sessionId].
     */
    data class UpdateUrlAction(val sessionId: String, val url: String) : ContentAction()

    /**
     * Updates the progress of the [ContentState] with the given [sessionId].
     */
    data class UpdateProgressAction(val sessionId: String, val progress: Int) : ContentAction()

    /**
     * Updates the title of the [ContentState] with the given [sessionId].
     */
    data class UpdateTitleAction(val sessionId: String, val title: String) : ContentAction()

    /**
     * Updates the loading state of the [ContentState] with the given [sessionId].
     */
    data class UpdateLoadingStateAction(val sessionId: String, val loading: Boolean) : ContentAction()

    /**
     * Updates the search terms of the [ContentState] with the given [sessionId].
     */
    data class UpdateSearchTermsAction(val sessionId: String, val searchTerms: String) : ContentAction()

    /**
     * Updates the [SecurityInfoState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateSecurityInfo(val sessionId: String, val securityInfo: SecurityInfoState) : ContentAction()
}
