/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper

/**
 * A [ContextWrapper] that holds the original [Activity] Context.
 *
 * @param baseContext see [ContextWrapper.getBaseContext].
 * @param originalContext the Context that the Activity was created with. This might be the same
 * as baseContext if a non-default value has not been set.
 */
class ActivityContextWrapper(
    baseContext: Context,
    val originalContext: Context = baseContext,
) : ContextWrapper(baseContext) {
    companion object {
        /**
         * Recursively try to retrieve the [ActivityContextWrapper.originalContext] from a wrapped
         * Activity Context.
         *
         * @param outerContext the Activity Context that may be wrapped in
         * an [ActivityContextWrapper].
         * @return the [ActivityContextWrapper.originalContext] or otherwise null if one
         * doesn't exist.
         */
        fun getOriginalContext(outerContext: Context): Context? {
            if (outerContext !is ContextWrapper) {
                return null
            }
            return if (outerContext is ActivityContextWrapper) {
                outerContext.originalContext
            } else {
                getOriginalContext(outerContext.baseContext)
            }
        }
    }
}
