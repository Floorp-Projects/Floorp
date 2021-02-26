/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.util.AttributeSet

/**
 * A browser tabs list that displays normal tabs.
 */
class NormalBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : BaseBrowserTrayList(context, attrs, defStyleAttr) {
    override val configuration: Configuration = Configuration(BrowserTabType.NORMAL)
}
