/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.utils

import android.os.Bundle
import android.os.Parcelable
import android.util.Log

/**
 * See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
 * experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
 * and we need to handle those defensively too.
 *
 * See bug 1090385 for more.
 */
class SafeBundle(private val bundle: Bundle) {

    fun getString(name: String): String? {
        try {
            return bundle.getString(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get bundle items: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get bundle items.", e)
            return null
        }

    }

    fun <T : Parcelable> getParcelable(name: String): T? {
        try {
            return bundle.getParcelable(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get bundle items: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get bundle items.", e)
            return null
        }

    }

    companion object {
        private val LOGTAG = SafeBundle::class.java.simpleName
    }
}
