/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser

import android.view.View
import android.widget.PopupWindow
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.Orientation
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.focus.ext.ifCustomTab
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.menu.browser.BrowserMenu

/**
 * This adapter bridges between `browser-toolbar` and the custom, legacy [BrowserMenu] in Focus.
 * It allows launching [BrowserMenu] from `BrowserToolbar`.
 */
class BrowserMenuControllerAdapter(
    private val fragment: BrowserFragment
) : MenuController, Observable<MenuController.Observer> by ObserverRegistry() {
    override fun dismiss() {
        // Not used.
    }

    override fun show(anchor: View, orientation: Orientation?): PopupWindow {
        val menu = BrowserMenu(fragment.requireContext(), fragment, fragment.tab.ifCustomTab()?.config)
        menu.show(anchor)

        return menu
    }

    override fun submitList(list: List<MenuCandidate>) {
        // Not used.
    }
}
