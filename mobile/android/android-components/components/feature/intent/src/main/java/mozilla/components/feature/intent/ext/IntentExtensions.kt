/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.ext

import android.content.Intent
import android.os.BadParcelableException
import mozilla.components.support.utils.SafeIntent

const val EXTRA_SESSION_ID = "activeSessionId"

/**
 * Retrieves [mozilla.components.browser.session.Session] ID from the intent.
 *
 * @return The session ID previously added with [putSessionId],
 * or null if no ID was found.
 */
fun Intent.getSessionId(): String? = getStringExtra(EXTRA_SESSION_ID)

/**
 * Retrieves [mozilla.components.browser.session.Session] ID from the intent.
 *
 * @return The session ID previously added with [putSessionId],
 * or null if no ID was found.
 */
fun SafeIntent.getSessionId(): String? = getStringExtra(EXTRA_SESSION_ID)

/**
 * Add [mozilla.components.browser.session.Session] ID to the intent.
 *
 * @param sessionId The session ID data value.
 *
 * @return Returns the same Intent object, for chaining multiple calls
 * into a single statement.
 *
 * @see [getSessionId]
 */
fun Intent.putSessionId(sessionId: String?): Intent {
    return putExtra(EXTRA_SESSION_ID, sessionId)
}

/**
 * Sanitizes the intent.  If the intent cannot be unparcelled, all extras are removed.
 * https://developer.android.com/guide/components/activities/parcelables-and-bundles
 *
 * @return Returns the sanitized Intent object.
 */
@Suppress("TooGenericExceptionCaught")
fun Intent.sanitize(): Intent {
    try {
        this.getBooleanExtra("TriggerUnparcel", false)
        return this
    } catch (e: BadParcelableException) {
        return this.replaceExtras(null)
    } catch (e: RuntimeException) {
        if (e.cause is ClassNotFoundException) {
            return this.replaceExtras(null)
        }

        throw e
    }
}
