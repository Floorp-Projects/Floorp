/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.annotation.SuppressLint
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.lib.push.firebase.AbstractFirebasePushService

/**
 * A singleton instance of the FirebasePushService needed for communicating between FCM and the
 * [AutoPushFeature].
 */
@SuppressLint("MissingFirebaseInstanceTokenRefresh") // Implemented internally.
class FirebasePushService : AbstractFirebasePushService()
