/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Service
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.IBinder
import mozilla.components.browser.state.store.BrowserStore

/**
 * A foreground service that will keep the process alive while we are playing media (with the app possibly in the
 * background) and shows an ongoing notification
 */
abstract class AbstractMediaService : Service() {
    protected abstract val store: BrowserStore

    private val delegate: MediaServiceDelegate by lazy {
        MediaServiceDelegate(
            context = this,
            service = this,
            store = store
        )
    }

    override fun onCreate() {
        super.onCreate()

        delegate.onCreate()
    }

    override fun onDestroy() {
        delegate.onDestroy()

        super.onDestroy()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        delegate.onStartCommand(intent)
        return START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        delegate.onTaskRemoved()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        internal const val ACTION_LAUNCH = "mozac.feature.media.service.LAUNCH"
        internal const val ACTION_PLAY = "mozac.feature.media.service.PLAY"
        internal const val ACTION_PAUSE = "mozac.feature.media.service.PAUSE"

        const val NOTIFICATION_TAG = "mozac.feature.media.foreground-service"
        const val PENDING_INTENT_TAG = "mozac.feature.media.pendingintent"
        const val ACTION_SWITCH_TAB = "mozac.feature.media.SWITCH_TAB"
        const val EXTRA_TAB_ID = "mozac.feature.media.TAB_ID"

        internal fun launchIntent(context: Context, cls: Class<*>) = Intent(ACTION_LAUNCH).apply {
            component = ComponentName(context, cls)
        }

        internal fun playIntent(context: Context, cls: Class<*>): Intent = Intent(ACTION_PLAY).apply {
            component = ComponentName(context, cls)
        }

        internal fun pauseIntent(context: Context, cls: Class<*>): Intent = Intent(ACTION_PAUSE).apply {
            component = ComponentName(context, cls)
        }
    }
}
