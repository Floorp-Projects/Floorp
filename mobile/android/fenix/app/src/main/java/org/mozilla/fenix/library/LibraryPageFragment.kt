/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.setToolbarColors

abstract class LibraryPageFragment<T> : Fragment() {

    abstract val selectedItems: Set<T>

    protected fun openItemsInNewTab(private: Boolean = false, toUrl: (T) -> String?) {
        context?.components?.useCases?.tabsUseCases?.let { tabsUseCases ->
            val addTab = if (private) tabsUseCases.addPrivateTab else tabsUseCases.addTab
            selectedItems.asSequence()
                .mapNotNull(toUrl)
                .forEach { url ->
                    addTab.invoke(url)
                }
        }

        (activity as HomeActivity).browsingModeManager.mode = BrowsingMode.fromBoolean(private)
        hideToolbar()
    }

    override fun onDetach() {
        super.onDetach()
        context?.let {
            activity?.findViewById<Toolbar>(R.id.navigationToolbar)?.setToolbarColors(
                it.getColorFromAttr(R.attr.primaryText),
                it.getColorFromAttr(R.attr.foundation)
            )
        }
    }
}
