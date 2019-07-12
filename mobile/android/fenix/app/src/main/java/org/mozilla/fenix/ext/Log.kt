/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.util.Log
import org.mozilla.fenix.BuildConfig

/**
 * Will print to `Log.d()` only when [BuildConfig.DEBUG] is enabled.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
@Suppress("NOTHING_TO_INLINE")
inline fun logDebug(tag: String, message: String) {
    if (BuildConfig.DEBUG) Log.d(tag, message)
}

/**
 * Will print to `Log.w()` only when [BuildConfig.DEBUG] is enabled.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
@Suppress("NOTHING_TO_INLINE")
inline fun logWarn(tag: String, message: String) {
    if (BuildConfig.DEBUG) Log.w(tag, message)
}

/**
 * Will print to `Log.w()` only when [BuildConfig.DEBUG] is enabled.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
@Suppress("NOTHING_TO_INLINE")
inline fun logWarn(tag: String, message: String, err: Throwable) {
    if (BuildConfig.DEBUG) Log.w(tag, message, err)
}

/**
 * Will print to `Log.e()` only when [BuildConfig.DEBUG] is enabled.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
@Suppress("NOTHING_TO_INLINE")
inline fun logErr(tag: String, message: String, err: Throwable) {
    if (BuildConfig.DEBUG) Log.e(tag, message, err)
}
