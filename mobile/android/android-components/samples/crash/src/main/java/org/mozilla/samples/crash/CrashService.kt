/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.widget.Toast
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.support.base.ids.SharedIdsHelper

private const val NOTIFICATION_CHANNEL_ID = "mozac.lib.crash.notification"
private const val NOTIFICATION_TAG = "mozac.lib.crash.foreground-service"
private const val DELAY_CRASH_MS = 10000L

/**
 * This service will wait 10 seconds and then crash. We need to wait some time because Android still allows to launch
 * an activity from a background service if the app was in the foreground a couple of seconds ago.
 */
class CrashService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    @Suppress("TooGenericExceptionThrown")
    override fun onCreate() {
        Toast.makeText(this, "Crashing from background soonish...", Toast.LENGTH_SHORT).show()

        // We need to put this service into foreground because otherwise Android may kill it (with no visible app UI)
        // before we can crash.
        startForeground(SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG), createNotification())

        GlobalScope.launch(Dispatchers.Main) {
            delay(DELAY_CRASH_MS)

            throw RuntimeException("Background crash")
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        return START_NOT_STICKY
    }

    private fun createNotification(): Notification {
        val channel = ensureChannelExists()

        return NotificationCompat.Builder(this, channel)
            .setContentTitle("Crash Service")
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setCategory(NotificationCompat.CATEGORY_ERROR)
            .build()
    }

    private fun ensureChannelExists(): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = getSystemService(
                Context.NOTIFICATION_SERVICE,
            ) as NotificationManager

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                "Crash Service",
                NotificationManager.IMPORTANCE_DEFAULT,
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }
}
