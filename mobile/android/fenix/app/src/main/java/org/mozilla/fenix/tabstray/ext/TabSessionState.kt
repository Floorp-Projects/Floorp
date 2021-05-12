/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.tabstray.browser.BrowserTrayList.BrowserTabType
import org.mozilla.fenix.tabstray.browser.BrowserTrayList.BrowserTabType.PRIVATE

fun TabSessionState.filterFromConfig(type: BrowserTabType): Boolean {
    val isPrivate = type == PRIVATE

    return content.private == isPrivate
}

fun TabSessionState.getTrayPosition(): Int =
    when (content.private) {
        true -> Page.PrivateTabs.ordinal
        false -> Page.NormalTabs.ordinal
    }
