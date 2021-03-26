/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor

/**
 * View holder for the private tabs tray list.
 */
class PrivateBrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : BaseBrowserTabViewHolder(containerView, interactor) {
    companion object {
        const val LAYOUT_ID = R.layout.private_browser_tray_list
    }
}
