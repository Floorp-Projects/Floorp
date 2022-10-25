/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.notification.CrashNotification
import mozilla.components.support.base.ids.SharedIdsHelper

private const val NOTIFICATION_TAG = "mozac.lib.crash.handlecrash"

/**
 * Service receiving native code crashes (from GeckoView).
 */
class CrashHandlerService : Service() {
    private val crashReporter: CrashReporter by lazy { CrashReporter.requireInstance }

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        crashReporter.logger.error("CrashHandlerService received native code crash")
        handleCrashIntent(intent)

        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent): IBinder? {
        // We don't provide binding, so return null
        return null
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleCrashIntent(
        intent: Intent,
        scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    ) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = CrashNotification.ensureChannelExists(this)
            val notification = NotificationCompat.Builder(this, channel)
                .setContentTitle(
                    getString(R.string.mozac_lib_gathering_crash_data_in_progress),
                )
                .setSmallIcon(R.drawable.mozac_lib_crash_notification)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setCategory(NotificationCompat.CATEGORY_ERROR)
                .setAutoCancel(true)
                .build()

            val notificationId = SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)
            startForeground(notificationId, notification)
        }

        scope.launch {
            intent.extras?.let { extras ->
                val crash = Crash.NativeCodeCrash.fromBundle(extras)
                CrashReporter.requireInstance.onCrash(this@CrashHandlerService, crash)
            } ?: crashReporter.logger.error("Received intent with null extras")

            stopSelf()
        }
    }
}
