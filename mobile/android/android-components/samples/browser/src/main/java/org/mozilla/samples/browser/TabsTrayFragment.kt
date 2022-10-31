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
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsAdapter
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.samples.browser.databinding.FragmentTabstrayBinding
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

        val binding = FragmentTabstrayBinding.bind(view)
        binding.toolbar.setNavigationIcon(R.drawable.mozac_ic_back)
        binding.toolbar.setNavigationOnClickListener {
            closeTabsTray()
        }

        binding.toolbar.inflateMenu(R.menu.tabstray_menu)
        binding.toolbar.setOnMenuItemClickListener {
            when (it.itemId) {
                R.id.newTab -> {
                    components.tabsUseCases.addTab.invoke("about:blank", selectTab = true)
                    closeTabsTray()
                }
            }
            true
        }

        val tabsAdapter = createTabsAdapter(view)
        binding.tabsTray.adapter = tabsAdapter
        binding.tabsTray.layoutManager = GridLayoutManager(context, 2)

        tabsFeature.set(
            feature = TabsFeature(
                tabsTray = tabsAdapter,
                store = components.store,
                onCloseTray = ::closeTabsTray,
            ),
            owner = this,
            view = view,
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

    private fun createTabsAdapter(view: View): TabsAdapter {
        val removeUseCase = RemoveTabWithUndoUseCase(
            components.tabsUseCases.removeTab,
            view,
            components.tabsUseCases.undo,
        )
        return TabsAdapter(
            thumbnailLoader = ThumbnailLoader(components.thumbnailStorage),
            delegate = object : TabsTray.Delegate {
                override fun onTabSelected(tab: TabSessionState, source: String?) {
                    components.tabsUseCases.selectTab(tab.id)
                    closeTabsTray()
                }

                override fun onTabClosed(tab: TabSessionState, source: String?) {
                    removeUseCase.invoke(tab.id)
                }
            },
        )
    }
}

private class RemoveTabWithUndoUseCase(
    private val actual: TabsUseCases.RemoveTabUseCase,
    private val view: View,
    private val undo: TabsUseCases.UndoTabRemovalUseCase,
) : TabsUseCases.RemoveTabUseCase {
    override fun invoke(tabId: String) {
        actual.invoke(tabId)
        showSnackbar()
    }

    private fun showSnackbar() {
        Snackbar.make(
            view,
            "Tab removed.",
            Snackbar.LENGTH_LONG,
        ).setAction(
            "Undo",
        ) {
            undo.invoke()
        }.show()
    }
}
