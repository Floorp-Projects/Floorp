/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.newSingleThreadContext

@ObsoleteCoroutinesApi
internal object Dispatchers {
    /**
     * A coroutine scope to make it easy to dispatch API calls off the main thread.
     * This needs to be a `var` so that our tests can override this.
     */
    var API = CoroutineScope(newSingleThreadContext("GleanAPIPool"))
}
