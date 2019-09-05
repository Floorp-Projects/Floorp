/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webnotifications

/**
 * A notification sent by the Web Notifications API.
 *
 * @property origin The website that fired this notification.
 * @property title Title of the notification to be displayed in the first row.
 * @property body Body of the notification to be displayed in the second row.
 * @property tag Tag used to identify the notification.
 * @property iconUrl Medium image to display in the notification.
 * Corresponds to [android.app.Notification.Builder.setLargeIcon].
 * @property vibrate Vibration pattern felt when the notification is displayed.
 * @property timestamp Time when the notification was created.
 * @property requireInteraction Preference flag that indicates the notification should remain
 * active until the user clicks or dismisses it.
 * @property silent Preference flag that indicates no sounds or vibrations should be made.
 * @property onClick Callback called with the selected action, or null if the main body of the
 * notification was clicked.
 * @property onClose Callback called when the notification is dismissed.
 */
@Suppress("Unused")
data class WebNotification(
    val origin: String,
    val title: String? = null,
    val body: String? = null,
    val tag: String? = null,
    val iconUrl: String? = null,
    val vibrate: LongArray = longArrayOf(),
    val timestamp: Long? = null,
    val requireInteraction: Boolean = false,
    val silent: Boolean = false,
    val onClick: () -> Unit,
    val onClose: () -> Unit
)
