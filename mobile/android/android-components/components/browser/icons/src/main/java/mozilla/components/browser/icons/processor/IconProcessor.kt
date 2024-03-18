/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize

/**
 * An [IconProcessor] implementation receives the [Icon] with the [IconRequest] and [IconRequest.Resource] after
 * the icon was loaded. The [IconProcessor] has the option to rewrite a loaded [Icon] and return a new instance.
 */
interface IconProcessor {
    fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize,
    ): Icon?
}
