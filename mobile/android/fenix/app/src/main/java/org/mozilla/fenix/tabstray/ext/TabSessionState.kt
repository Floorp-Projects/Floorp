/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.tabstray.browser.BaseBrowserTrayList.BrowserTabType.PRIVATE
import org.mozilla.fenix.tabstray.browser.BaseBrowserTrayList.Configuration

fun TabSessionState.filterFromConfig(configuration: Configuration): Boolean {
    val isPrivate = configuration.browserTabType == PRIVATE

    return content.private == isPrivate
}
