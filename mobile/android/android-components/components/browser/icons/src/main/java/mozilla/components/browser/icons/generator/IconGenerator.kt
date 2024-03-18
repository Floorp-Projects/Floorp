/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.generator

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * A [IconGenerator] implementation can generate a [Bitmap] for an [IconRequest]. It's a fallback if no icon could be
 * loaded for a specific URL.
 */
interface IconGenerator {
    fun generate(context: Context, request: IconRequest): Icon
}
