package org.mozilla.geckoview.test.crash

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.support.v4.app.NotificationCompat
import java.util.concurrent.SynchronousQueue

class CrashTestHandler : Service() {
    companion object {
        val LOGTAG = "CrashTestHandler"
        val queue = SynchronousQueue<Intent>()
    }

    var notification: Notification? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && notification == null) {
            val channelId = "test"
            val channel = NotificationChannel(channelId, "Crashes", NotificationManager.IMPORTANCE_LOW)
            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager!!.createNotificationChannel(channel)

            notification = NotificationCompat.Builder(this, channelId)
                    .setSmallIcon(android.R.drawable.ic_dialog_alert)
                    .setContentTitle("Crash Test Service")
                    .setContentText("I'm just here so I don't get killed")
                    .setDefaults(Notification.DEFAULT_ALL)
                    .setAutoCancel(true)
                    .setOngoing(false)
                    .build()

            startForeground(42, notification)
        }

        queue.put(intent)
        return super.onStartCommand(intent, flags, startId)
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }
}
