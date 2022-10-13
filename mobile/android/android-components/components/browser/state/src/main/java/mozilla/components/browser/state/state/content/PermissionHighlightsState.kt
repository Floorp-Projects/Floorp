/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * Value type that represents any information about permissions that should
 * be brought to user's attention.
 *
 * @property notificationChanged indicates if the notification permission has changed from
 * its default value.
 * @property cameraChanged indicates if the camera permission has changed from its default value.
 * @property locationChanged indicates if the location permission has changed from its default value.
 * @property microphoneChanged indicates if the microphone permission has changed from its default value.
 * @property persistentStorageChanged indicates if the persistent storage permission has
 * changed from its default value.
 * @property mediaKeySystemAccessChanged indicates if the media key systemAccess
 * permission has changed from its default value.
 * @property autoPlayAudibleChanged indicates if the autoplay audible permission has changed
 * from its default value.
 * @property autoPlayInaudibleChanged indicates if the autoplay inaudible permission has changed
 * from its default value.
 * @property autoPlayAudibleBlocking indicates if the autoplay audible setting disabled some
 * web content from playing.
 * @property autoPlayInaudibleBlocking indicates if the autoplay inaudible setting disabled
 * some web content from playing.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class PermissionHighlightsState(
    val notificationChanged: Boolean = false,
    val cameraChanged: Boolean = false,
    val locationChanged: Boolean = false,
    val microphoneChanged: Boolean = false,
    val persistentStorageChanged: Boolean = false,
    val mediaKeySystemAccessChanged: Boolean = false,
    val autoPlayAudibleChanged: Boolean = false,
    val autoPlayInaudibleChanged: Boolean = false,
    val autoPlayAudibleBlocking: Boolean = false,
    val autoPlayInaudibleBlocking: Boolean = false,
) : Parcelable {
    val isAutoPlayBlocking get() = autoPlayAudibleBlocking || autoPlayInaudibleBlocking
    val permissionsChanged
        get() = notificationChanged || cameraChanged || locationChanged ||
            microphoneChanged || persistentStorageChanged || mediaKeySystemAccessChanged ||
            autoPlayAudibleChanged || autoPlayInaudibleChanged
}
