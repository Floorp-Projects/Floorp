/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.internal

import android.graphics.Bitmap
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest

/**
 * Helper that creates a suspendable lambda to either load a static bitmap ([default]) or from [BrowserIcons] if not
 * null.
 */
internal fun BrowserIcons?.loadLambda(
    url: String,
    default: Bitmap? = null
): (suspend (width: Int, height: Int) -> Bitmap?)? {
    if (default != null) {
        return { _, _ -> default }
    }

    if (this != null) {
        return { _, _ -> loadIcon(IconRequest(url)).await().bitmap }
    }

    return null
}
