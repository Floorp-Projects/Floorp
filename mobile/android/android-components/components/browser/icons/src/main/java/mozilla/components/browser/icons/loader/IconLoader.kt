/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * A loader that can load an icon from an [IconRequest.Resource].
 */
interface IconLoader {
    /**
     * Tries to load the [IconRequest.Resource] for the given [IconRequest].
     */
    fun load(request: IconRequest, resource: IconRequest.Resource): ByteArray?

    val source: Icon.Source
}
