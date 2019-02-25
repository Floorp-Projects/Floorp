/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

/**
 * An interface for all push services to implement that allows [Push] to manage.
 */
interface PushService {

    /**
     * Start the push service.
     */
    fun start()

    /**
     * Stop the push service.
     */
    fun stop()

    /**
     * Request registration renewal from push service. This leads to the Push service sending a new registration
     * token if successful.
     */
    fun renewRegistration()
}
