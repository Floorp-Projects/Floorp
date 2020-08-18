/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_synced_tabs.*
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.SyncedTabsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment

class SyncedTabsFragment : LibraryPageFragment<Tab>() {
    private val syncedTabsFeature = ViewBoundFeatureWrapper<SyncedTabsFeature>()

    init {
        // Sanity-check: Remove this class when the feature flag is always enabled.
        FeatureFlags.syncedTabsInTabsTray
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_synced_tabs, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val backgroundServices = requireContext().components.backgroundServices

        syncedTabsFeature.set(
            feature = SyncedTabsFeature(
                context = requireContext(),
                storage = backgroundServices.syncedTabsStorage,
                accountManager = backgroundServices.accountManager,
                view = synced_tabs_layout,
                lifecycleOwner = this.viewLifecycleOwner,
                onTabClicked = ::handleTabClicked
            ),
            owner = this,
            view = view
        )
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_synced_tabs))
    }

    private fun handleTabClicked(tab: Tab) {

        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = tab.active().url,
            newTab = true,
            from = BrowserDirection.FromSyncedTabs
        )
    }

    override val selectedItems: Set<Tab>
        get() = emptySet()
}
