/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.preparer

import android.content.Context
import mozilla.components.browser.icons.IconRequest

/**
 * [IconPreprarer] implementation implementation that will add known resource URLs (from a disk cache) to the request
 * if the request doesn't contain a list of resources yet.
 */
class DiskIconPreparer(
    private val cache: PreparerDiskCache,
) : IconPreprarer {
    interface PreparerDiskCache {
        fun getResources(context: Context, request: IconRequest): List<IconRequest.Resource>
    }

    override fun prepare(context: Context, request: IconRequest): IconRequest {
        return if (request.resources.isEmpty()) {
            // Only try to load resources for this request from disk if there are no resources attached to this
            // request yet: Avoid disk reads for *every* request - especially if they can be satisfied by the memory
            // cache.
            val resources = cache.getResources(context, request)
            request.copy(resources = resources)
        } else {
            request
        }
    }
}
