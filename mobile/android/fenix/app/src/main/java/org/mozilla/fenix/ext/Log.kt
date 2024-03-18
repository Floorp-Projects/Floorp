/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("NOTHING_TO_INLINE")

package org.mozilla.fenix.ext

import android.util.Log
import org.mozilla.fenix.Config

/**
 * Will print to `Log.d()` only for debug release channels.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
inline fun logDebug(tag: String, message: String) {
    if (Config.channel.isDebug) Log.d(tag, message)
}

/**
 * Will print to `Log.w()` only for debug release channels.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
inline fun logWarn(tag: String, message: String) {
    if (Config.channel.isDebug) Log.w(tag, message)
}

/**
 * Will print to `Log.w()` only for debug release channels.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
inline fun logWarn(tag: String, message: String, err: Throwable) {
    if (Config.channel.isDebug) Log.w(tag, message, err)
}

/**
 * Will print to `Log.e()` only for debug release channels.
 *
 * Meant to be used for logs that should not be visible in the production app.
 */
inline fun logErr(tag: String, message: String, err: Throwable) {
    if (Config.channel.isDebug) Log.e(tag, message, err)
}
