/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webnotifications

/**
 * Notifies applications or other components of engine events related to web
 * notifications e.g. an notification is to be shown or is to be closed
 */
interface WebNotificationDelegate {
    /**
     * Invoked when a web notification is to be shown.
     *
     * @param webNotification The web notification intended to be shown.
     */
    fun onShowNotification(webNotification: WebNotification) = Unit

    /**
     * Invoked when a web notification is to be closed.
     *
     * @param webNotification The web notification intended to be closed.
     */
    fun onCloseNotification(webNotification: WebNotification) = Unit
}
