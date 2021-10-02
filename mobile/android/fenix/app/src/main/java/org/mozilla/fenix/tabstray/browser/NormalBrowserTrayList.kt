/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.ConcatAdapter
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.isNormalTabActive
import org.mozilla.fenix.tabstray.ext.isNormalTabActiveWithSearchTerm
import org.mozilla.fenix.tabstray.ext.isNormalTabActiveWithoutSearchTerm
import org.mozilla.fenix.tabstray.ext.isNormalTabWithoutSearchTerm
import org.mozilla.fenix.tabstray.ext.isNormalTabWithSearchTerm
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter
import java.util.concurrent.TimeUnit

/**
 * The time until which a tab is considered in-active (in days).
 */
const val DEFAULT_ACTIVE_DAYS = 14L

/**
 * The maximum time from when a tab was created or accessed until it is considered "inactive".
 */
val maxActiveTime = TimeUnit.DAYS.toMillis(DEFAULT_ACTIVE_DAYS)

class NormalBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AbstractBrowserTrayList(context, attrs, defStyleAttr) {

    private val concatAdapter by lazy { adapter as ConcatAdapter }

    override val tabsFeature by lazy {
        val tabsAdapter = concatAdapter.browserAdapter
        val inactiveTabsEnabled = context.settings().inactiveTabsAreEnabled
        val searchTermTabGroupsAreEnabled = context.settings().searchTermTabGroupsAreEnabled
        val tabFilter: (TabSessionState) -> Boolean = {
            when {
                searchTermTabGroupsAreEnabled && inactiveTabsEnabled ->
                    it.isNormalTabActiveWithoutSearchTerm(maxActiveTime)

                inactiveTabsEnabled -> it.isNormalTabActive(maxActiveTime)

                searchTermTabGroupsAreEnabled -> it.isNormalTabWithoutSearchTerm()

                else -> !it.content.private
            }
        }

        TabsFeature(
            tabsAdapter,
            context.components.core.store,
            selectTabUseCase,
            removeTabUseCase,
            tabFilter,
            {}
        )
    }

    private val searchTermFeature by lazy {
        val store = context.components.core.store
        val inactiveTabsEnabled = context.settings().inactiveTabsAreEnabled
        val searchTermTabGroupsAreEnabled = context.settings().searchTermTabGroupsAreEnabled
        val tabFilter: (TabSessionState) -> Boolean = {
            when {
                searchTermTabGroupsAreEnabled && inactiveTabsEnabled ->
                    it.isNormalTabActiveWithSearchTerm(maxActiveTime)

                searchTermTabGroupsAreEnabled -> it.isNormalTabWithSearchTerm()

                else -> false
            }
        }
        val tabsAdapter = concatAdapter.tabGroupAdapter

        TabsFeature(
            tabsAdapter,
            store,
            selectTabUseCase,
            removeTabUseCase,
            tabFilter,
            {}
        )
    }

    /**
     * NB: The setup for this feature is a bit complicated without a better dependency injection
     * solution to scope it down to just this view.
     */
    private val inactiveFeature by lazy {
        val store = context.components.core.store
        val tabFilter: (TabSessionState) -> Boolean = filter@{
            if (!context.settings().inactiveTabsAreEnabled) {
                return@filter false
            }
            it.isNormalTabInactive(maxActiveTime)
        }
        val tabsAdapter = concatAdapter.inactiveTabsAdapter.apply {
            inactiveTabsInteractor = DefaultInactiveTabsInteractor(
                InactiveTabsController(store, tabFilter, this, context.components.analytics.metrics)
            )
        }

        TabsFeature(
            tabsAdapter,
            store,
            selectTabUseCase,
            removeTabUseCase,
            tabFilter,
            {}
        )
    }

    private val touchHelper by lazy {
        TabsTouchHelper(
            observable = concatAdapter.browserAdapter,
            onViewHolderTouched = {
                it is TabViewHolder && swipeToDelete.isSwipeable
            },
            onViewHolderDraw = { context.components.settings.gridTabView.not() }
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        inactiveFeature.start()
        searchTermFeature.start()
        tabsFeature.start()

        touchHelper.attachToRecyclerView(this)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        tabsFeature.stop()
        searchTermFeature.stop()
        inactiveFeature.stop()

        touchHelper.attachToRecyclerView(null)
    }
}
