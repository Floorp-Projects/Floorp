/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.GridLayoutManager
import kotlinx.android.synthetic.main.fragment_tabstray.tabsTray
import kotlinx.android.synthetic.main.fragment_tabstray.toolbar
import mozilla.components.browser.tabstray.TabsAdapter
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.samples.browser.ext.components

/**
 * A fragment for displaying the tabs tray.
 */
class TabsTrayFragment : Fragment(), UserInteractionHandler {
    private val tabsFeature: ViewBoundFeatureWrapper<TabsFeature> = ViewBoundFeatureWrapper()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View =
        inflater.inflate(R.layout.fragment_tabstray, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        toolbar.setNavigationIcon(R.drawable.mozac_ic_back)
        toolbar.setNavigationOnClickListener {
            closeTabsTray()
        }

        toolbar.inflateMenu(R.menu.tabstray_menu)
        toolbar.setOnMenuItemClickListener {
            when (it.itemId) {
                R.id.newTab -> {
                    components.tabsUseCases.addTab.invoke("about:blank", selectTab = true)
                    closeTabsTray()
                }
            }
            true
        }

        val tabsAdapter = createTabsAdapter()
        tabsTray.adapter = tabsAdapter
        tabsTray.layoutManager = GridLayoutManager(context, 2)

        tabsFeature.set(
            feature = TabsFeature(
                tabsTray = tabsAdapter,
                store = components.store,
                selectTabUseCase = components.tabsUseCases.selectTab,
                removeTabUseCase = components.tabsUseCases.removeTab,
                closeTabsTray = ::closeTabsTray
            ),
            owner = this,
            view = view
        )
    }

    override fun onBackPressed(): Boolean {
        closeTabsTray()
        return true
    }

    private fun closeTabsTray() {
        activity?.supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, BrowserFragment.create())
            commit()
        }
    }

    private fun createTabsAdapter(): TabsAdapter {
        val thumbnailLoader = ThumbnailLoader(components.thumbnailStorage)
        return TabsAdapter(thumbnailLoader)
    }
}
