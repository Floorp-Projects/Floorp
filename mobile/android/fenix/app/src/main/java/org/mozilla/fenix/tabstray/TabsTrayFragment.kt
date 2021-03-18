/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.synthetic.main.component_tabstray2.*
import kotlinx.android.synthetic.main.component_tabstray2.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.DefaultBrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.RemoveTabUseCaseWrapper
import org.mozilla.fenix.tabstray.browser.SelectTabUseCaseWrapper

class TabsTrayFragment : AppCompatDialogFragment(), TabsTrayInteractor {

    lateinit var behavior: BottomSheetBehavior<ConstraintLayout>

    private val selectTabUseCase by lazy {
        SelectTabUseCaseWrapper(
            requireComponents.analytics.metrics,
            requireComponents.useCases.tabsUseCases.selectTab
        ) {
            navigateToBrowser()
        }
    }

    private val removeUseCases by lazy {
        RemoveTabUseCaseWrapper(requireComponents.analytics.metrics
        ) {
            tabRemoved(it)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val containerView = inflater.inflate(R.layout.fragment_tab_tray_dialog, container, false)
        val view: View = LayoutInflater.from(containerView.context)
            .inflate(R.layout.component_tabstray2, containerView as ViewGroup, true)

        behavior = BottomSheetBehavior.from(view.tab_wrapper)

        return containerView
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val browserTrayInteractor = DefaultBrowserTrayInteractor(
            this,
            selectTabUseCase,
            removeUseCases
        )

        setupPager(view.context, this, browserTrayInteractor)
    }

    override fun setCurrentTrayPosition(position: Int) {
        tabsTray.currentItem = position
    }

    override fun navigateToBrowser() {
        dismissAllowingStateLoss()

        val navController = findNavController()

        if (navController.currentDestination?.id == R.id.browserFragment) {
            return
        }

        if (!navController.popBackStack(R.id.browserFragment, false)) {
            navController.navigate(R.id.browserFragment)
        }
    }

    override fun tabRemoved(tabId: String) {
        // TODO re-implement these methods
        // showUndoSnackbarForTab(sessionId)
        // removeIfNotLastTab(sessionId)

        // Temporary
        requireComponents.useCases.tabsUseCases.removeTab(tabId)
    }

    private fun setupPager(
        context: Context,
        trayInteractor: TabsTrayInteractor,
        browserInteractor: BrowserTrayInteractor
    ) {
        tabsTray.apply {
            adapter = TrayPagerAdapter(context, trayInteractor, browserInteractor)
            isUserInputEnabled = false
        }

        tab_layout.addOnTabSelectedListener(TabLayoutObserver(trayInteractor))
    }
}

/**
 * An observer for the [TabLayout] used for the Tabs Tray.
 */
internal class TabLayoutObserver(
    private val interactor: TabsTrayInteractor
) : TabLayout.OnTabSelectedListener {
    override fun onTabSelected(tab: TabLayout.Tab) {
        interactor.setCurrentTrayPosition(tab.position)
    }

    override fun onTabUnselected(tab: TabLayout.Tab) = Unit
    override fun onTabReselected(tab: TabLayout.Tab) = Unit
}
