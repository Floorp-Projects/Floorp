/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.Bundle
import android.os.Parcelable
import mozilla.components.support.base.log.logger.Logger

/**
 * See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
 * experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
 * and we need to handle those defensively too.
 *
 * See bug 1090385 for more.
 */
class SafeBundle(private val bundle: Bundle) {

    @SuppressWarnings("TooGenericExceptionCaught")
    fun getString(name: String): String? {
        return try {
            bundle.getString(name)
        } catch (e: OutOfMemoryError) {
            Logger.warn("Couldn't get bundle items: OOM. Malformed?")
            null
        } catch (e: RuntimeException) {
            Logger.warn("Couldn't get bundle items.", e)
            null
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    fun <T : Parcelable> getParcelable(name: String): T? {
        return try {
            bundle.getParcelable(name)
        } catch (e: OutOfMemoryError) {
            Logger.warn("Couldn't get bundle items: OOM. Malformed?")
            null
        } catch (e: RuntimeException) {
            Logger.warn("Couldn't get bundle items.", e)
            null
        }
    }
}
