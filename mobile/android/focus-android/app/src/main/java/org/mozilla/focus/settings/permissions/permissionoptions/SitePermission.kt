/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions.permissionoptions

import android.Manifest
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

@Parcelize
enum class SitePermission(val androidPermissionsList: Array<String>) : Parcelable {
    CAMERA(arrayOf(Manifest.permission.CAMERA)),
    LOCATION(arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION)),
    MICROPHONE(arrayOf(Manifest.permission.RECORD_AUDIO)),
    NOTIFICATION(emptyArray()),
    AUTOPLAY(emptyArray()),
    AUTOPLAY_AUDIBLE(emptyArray()),
    AUTOPLAY_INAUDIBLE(emptyArray()),
    MEDIA_KEY_SYSTEM_ACCESS(emptyArray()),
}
