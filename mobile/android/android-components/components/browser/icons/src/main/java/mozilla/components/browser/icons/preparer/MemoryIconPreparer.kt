/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.preparer

import android.content.Context
import mozilla.components.browser.icons.IconRequest

/**
 * An [IconPreprarer] implementation that will add known resource URLs (from an in-memory cache) to the request if the
 * request doesn't contain a list of resources yet.
 */
class MemoryIconPreparer(
    private val cache: PreparerMemoryCache,
) : IconPreprarer {
    interface PreparerMemoryCache {
        fun getResources(request: IconRequest): List<IconRequest.Resource>
    }

    override fun prepare(context: Context, request: IconRequest): IconRequest {
        return if (request.resources.isNotEmpty()) {
            request
        } else {
            val resources = cache.getResources(request)
            request.copy(resources = resources)
        }
    }
}
