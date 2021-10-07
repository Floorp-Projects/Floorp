/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.ext.toTabs
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation for connecting a tabs tray implementation with the session module.
 *
 * @param defaultTabsFilter A tab filter that is used for the initial presenting of tabs that will be used by
 * [TabsFeature.filterTabs] by default as well.
 */
@Suppress("LongParameterList")
class TabsFeature(
    private val tabsTray: TabsTray,
    private val store: BrowserStore,
    selectTabUseCase: TabsUseCases.SelectTabUseCase,
    removeTabUseCase: TabsUseCases.RemoveTabUseCase,
    private val defaultTabsFilter: (TabSessionState) -> Boolean = { true },
    closeTabsTray: () -> Unit
) : LifecycleAwareFeature {
    @VisibleForTesting
    internal var presenter = TabsTrayPresenter(
        tabsTray,
        store,
        defaultTabsFilter,
        closeTabsTray
    )

    @VisibleForTesting
    internal var interactor = TabsTrayInteractor(
        tabsTray,
        selectTabUseCase,
        removeTabUseCase,
        closeTabsTray
    )

    override fun start() {
        presenter.start()
        interactor.start()
    }

    override fun stop() {
        presenter.stop()
        interactor.stop()
    }

    /**
     * Filter the list of tabs using [tabsFilter].
     *
     * @param tabsFilter A filter function returning `true` for all tabs that should be displayed in
     * the tabs tray. Uses the [defaultTabsFilter] if none is provided.
     */
    fun filterTabs(tabsFilter: (TabSessionState) -> Boolean = defaultTabsFilter) {
        presenter.tabsFilter = tabsFilter

        val state = store.state
        tabsTray.updateTabs(state.toTabs(tabsFilter))
    }
}
