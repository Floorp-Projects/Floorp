/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.view.updatePadding
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.view.*
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R

class TabTrayDialogFragment : AppCompatDialogFragment(), TabTrayInteractor {
    interface Interactor {
        fun onTabSelected(tab: Tab)
        fun onNewTabTapped(private: Boolean)
        fun onTabClosed(tab: Tab)
    }

    private lateinit var tabTrayView: TabTrayView
    var interactor: Interactor? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_tab_tray_dialog, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        tabTrayView = TabTrayView(
            view.tabLayout,
            this,
            (activity as HomeActivity).browsingModeManager.mode.isPrivate
        )

        tabLayout.setOnClickListener { dismissAllowingStateLoss() }

        view.tabLayout.setOnApplyWindowInsetsListener { v, insets ->
            v.updatePadding(
                left = insets.systemWindowInsetLeft,
                right = insets.systemWindowInsetRight,
                bottom = insets.systemWindowInsetBottom
            )

            tabTrayView.view.tab_wrapper.updatePadding(
                bottom = insets.systemWindowInsetBottom
            )

            insets
        }
    }

    override fun onTabClosed(tab: Tab) {
        interactor?.onTabClosed(tab)
    }

    override fun onTabSelected(tab: Tab) {
        interactor?.onTabSelected(tab)
    }

    override fun onNewTabTapped(private: Boolean) {
        interactor?.onNewTabTapped(private)
    }

    override fun onTabTrayDismissed() {
        dismissAllowingStateLoss()
    }
}
