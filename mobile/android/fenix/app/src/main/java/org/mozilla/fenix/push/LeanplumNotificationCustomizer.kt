/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.app.Notification
import android.os.Bundle
import androidx.core.app.NotificationCompat
import com.leanplum.LeanplumPushNotificationCustomizer
import org.mozilla.fenix.R

/**
 * Notification customizer for incoming Leanplum push messages.
 */
class LeanplumNotificationCustomizer : LeanplumPushNotificationCustomizer {
    override fun customize(
        builder: NotificationCompat.Builder,
        notificationPayload: Bundle?
    ) {
        builder.setSmallIcon(R.drawable.ic_status_logo)
    }

    // Do not implement if unless we want to support 2 lines of text in the BigPicture style.
    // See: https://docs.leanplum.com/docs/customize-your-push-notifications-sample-android
    override fun customize(
        builder: Notification.Builder?,
        notificationPayload: Bundle?,
        notificationStyle: Notification.Style?
    ) = Unit // no-op
}
