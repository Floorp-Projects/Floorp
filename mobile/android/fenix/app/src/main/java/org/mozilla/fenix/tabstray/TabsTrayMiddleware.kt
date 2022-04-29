/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.annotation.VisibleForTesting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.SearchTerms
import org.mozilla.fenix.GleanMetrics.TabsTray

/**
 * [Middleware] that reacts to various [TabsTrayAction]s.
 */
class TabsTrayMiddleware : Middleware<TabsTrayState, TabsTrayAction> {

    private var shouldReportInactiveTabMetrics: Boolean = true
    private var shouldReportSearchGroupMetrics: Boolean = true

    override fun invoke(
        context: MiddlewareContext<TabsTrayState, TabsTrayAction>,
        next: (TabsTrayAction) -> Unit,
        action: TabsTrayAction
    ) {
        next(action)

        when (action) {
            is TabsTrayAction.UpdateInactiveTabs -> {
                if (shouldReportInactiveTabMetrics) {
                    shouldReportInactiveTabMetrics = false

                    TabsTray.hasInactiveTabs.record(TabsTray.HasInactiveTabsExtra(action.tabs.size))
                    Metrics.inactiveTabsCount.set(action.tabs.size.toLong())
                }
            }
            is TabsTrayAction.UpdateTabPartitions -> {
                if (shouldReportSearchGroupMetrics) {
                    shouldReportSearchGroupMetrics = false
                    val tabGroups = action.tabPartition?.tabGroups ?: emptyList()

                    SearchTerms.numberOfSearchTermGroup.record(
                        SearchTerms.NumberOfSearchTermGroupExtra(
                            tabGroups.size.toString()
                        )
                    )

                    if (tabGroups.isNotEmpty()) {
                        val tabsPerGroup = tabGroups.map { it.tabIds.size }
                        val averageTabsPerGroup = tabsPerGroup.average()
                        SearchTerms.averageTabsPerGroup.record(
                            SearchTerms.AverageTabsPerGroupExtra(
                                averageTabsPerGroup.toString()
                            )
                        )

                        val tabGroupSizeMapping = tabsPerGroup.map { generateTabGroupSizeMappedValue(it) }
                        SearchTerms.groupSizeDistribution.accumulateSamples(tabGroupSizeMapping.toLongArray())
                    }
                }
            }
            is TabsTrayAction.EnterSelectMode -> {
                TabsTray.enterMultiselectMode.record(TabsTray.EnterMultiselectModeExtra(false))
            }
            is TabsTrayAction.AddSelectTab -> {
                TabsTray.enterMultiselectMode.record(TabsTray.EnterMultiselectModeExtra(true))
            }
            else -> {
                // no-op
            }
        }
    }

    @Suppress("MagicNumber")
    @VisibleForTesting
    /**
     * This follows the logic outlined in metrics.yaml for "search_terms.group_size_distribution"
     */
    internal fun generateTabGroupSizeMappedValue(size: Int): Long =
        when (size) {
            2 -> 1L
            in 3..5 -> 2L
            in 6..10 -> 3L
            else -> 4L
        }
}
