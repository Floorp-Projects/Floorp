/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.UpdateInactiveExpanded
import org.mozilla.fenix.ext.potentialInactiveTabs
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.TabsTray as TabsTrayMetrics

/**
 * Contract for how all user interactions with the Inactive Tabs feature are to be handled.
 */
interface InactiveTabsController {

    /**
     * Opens the provided inactive tab.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun handleInactiveTabClicked(tab: TabSessionState)

    /**
     * Closes the provided inactive tab.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun handleCloseInactiveTabClicked(tab: TabSessionState)

    /**
     * Expands or collapses the inactive tabs section.
     *
     * @param expanded true when the tap should expand the inactive section.
     */
    fun handleInactiveTabsHeaderClicked(expanded: Boolean)

    /**
     * Dismisses the inactive tabs auto-close dialog.
     */
    fun handleInactiveTabsAutoCloseDialogDismiss()

    /**
     * Enables the inactive tabs auto-close feature with a default time period.
     */
    fun handleEnableInactiveTabsAutoCloseClicked()

    /**
     * Deletes all inactive tabs.
     */
    fun handleDeleteAllInactiveTabsClicked()
}

/**
 * Default behavior for handling all user interactions with the Inactive Tabs feature.
 *
 * @param appStore [AppStore] used to dispatch any [AppAction].
 * @param settings [Settings] used to update any user preferences.
 * @param browserStore [BrowserStore] used to obtain all inactive tabs.
 * @param tabsUseCases [TabsUseCases] used to perform the deletion of all inactive tabs.
 * @param showUndoSnackbar Invoked when deleting all inactive tabs.
 */
class DefaultInactiveTabsController(
    private val appStore: AppStore,
    private val settings: Settings,
    private val browserStore: BrowserStore,
    private val tabsUseCases: TabsUseCases,
    private val showUndoSnackbar: (Boolean) -> Unit,
) : InactiveTabsController {

    override fun handleInactiveTabClicked(tab: TabSessionState) {
        TabsTrayMetrics.openInactiveTab.add()
    }

    override fun handleCloseInactiveTabClicked(tab: TabSessionState) {
        TabsTrayMetrics.closeInactiveTab.add()
    }

    override fun handleInactiveTabsHeaderClicked(expanded: Boolean) {
        appStore.dispatch(UpdateInactiveExpanded(expanded))

        when (expanded) {
            true -> TabsTrayMetrics.inactiveTabsExpanded.record(NoExtras())
            false -> TabsTrayMetrics.inactiveTabsCollapsed.record(NoExtras())
        }
    }

    override fun handleInactiveTabsAutoCloseDialogDismiss() {
        markDialogAsShown()
        TabsTrayMetrics.autoCloseDimissed.record(NoExtras())
    }

    override fun handleEnableInactiveTabsAutoCloseClicked() {
        markDialogAsShown()
        settings.closeTabsAfterOneMonth = true
        settings.closeTabsAfterOneWeek = false
        settings.closeTabsAfterOneDay = false
        settings.manuallyCloseTabs = false
        TabsTrayMetrics.autoCloseTurnOnClicked.record(NoExtras())
    }

    override fun handleDeleteAllInactiveTabsClicked() {
        TabsTrayMetrics.closeAllInactiveTabs.record(NoExtras())
        browserStore.state.potentialInactiveTabs.map { it.id }.let {
            tabsUseCases.removeTabs(it)
        }
        showUndoSnackbar(false)
    }

    /**
     * Marks the dialog as shown and to not be displayed again.
     */
    private fun markDialogAsShown() {
        settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true
    }
}
