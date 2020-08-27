/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webnotifications

/**
 * A notification sent by the Web Notifications API.
 *
 * @property title Title of the notification to be displayed in the first row.
 * @property tag Tag used to identify the notification.
 * @property body Body of the notification to be displayed in the second row.
 * @property sourceUrl The URL of the page or Service Worker that generated the notification.
 * @property iconUrl Large icon url to display in the notification.
 * Corresponds to [android.app.Notification.Builder.setLargeIcon].
 * @property direction Preference for text direction.
 * @property lang language of the notification.
 * @property requireInteraction Preference flag that indicates the notification should remain.
 * @property timestamp Time when the notification was created.
 */
data class WebNotification(
    val title: String?,
    val tag: String,
    val body: String?,
    val sourceUrl: String?,
    val iconUrl: String?,
    val direction: String?,
    val lang: String?,
    val requireInteraction: Boolean,
    val timestamp: Long = System.currentTimeMillis()
)
