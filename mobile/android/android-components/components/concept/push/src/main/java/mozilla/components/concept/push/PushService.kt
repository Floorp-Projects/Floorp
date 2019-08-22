/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

import android.content.Context

/**
 * Implemented by push services like Firebase Cloud Messaging and Amazon Device Messaging SDKs to allow
 * the [PushProcessor] to manage their lifecycle.
 */
interface PushService {

    /**
     * Starts the push service.
     */
    fun start(context: Context)

    /**
     * Stops the push service.
     */
    fun stop()

    /**
     * Tells the push service to delete the registration token.
     */
    fun deleteToken()

    /**
     * If the push service is support on the device.
     */
    fun isServiceAvailable(context: Context): Boolean
}
