/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Context
import androidx.annotation.VisibleForTesting

/**
 * Caches the list of browsers installed on a user's device.
 *
 * BrowsersCache caches the list of installed browsers is gathered lazily when it is first accessed
 * after initial creation or invalidation. For that reason, a context is required every time
 * the cache is accessed.
 *
 * Users are responsible for invalidating the cache at the appropriate time. It is left up to the
 * user to determine appropriate policies for maintaining the validity of the cache. If, when the
 * cache is accessed, it is filled, the contents will be returned. As mentioned above, the cache
 * will be lazily refilled after invalidation. In other words, invalidation is O(1).
 *
 * This cache is threadsafe.
 */
object BrowsersCache {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var cachedBrowsers: Browsers? = null

    /**
     * Return installed browsers if cache exist.  If not, Collect information about all installed
     * browsers and return a [Browsers] object containing that data.
     */
    @Synchronized
    fun all(context: Context): Browsers {
        run {
            val cachedBrowsers = cachedBrowsers
            if (cachedBrowsers != null) {
                return cachedBrowsers
            }
        }
        return Browsers.all(context).also {
            this.cachedBrowsers = it
        }
    }

    /**
     * Remove installed browsers cache
     */
    @Synchronized
    fun resetAll() {
        cachedBrowsers = null
    }
}
