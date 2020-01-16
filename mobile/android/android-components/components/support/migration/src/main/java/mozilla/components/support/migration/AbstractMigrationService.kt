/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationStore

private const val NOTIFICATION_TAG = "mozac.support.migration.notification"
private const val NOTIFICATION_CHANNEL_ID = "mozac.support.migration.generic"

private const val TEMPORARY_NOTIFICATION_CHANNEL_NAME = "Migration"

/**
 * Abstract implementation of a background service running a configured [FennecMigrator].
 *
 * An application using this implementation needs to extend this class and provide a
 * [FennecMigrator] instance.
 */
abstract class AbstractMigrationService : Service() {
    protected abstract val migrator: FennecMigrator
    protected abstract val store: MigrationStore

    private val logger = Logger("MigrationService")
    private val scope = MainScope()

    override fun onCreate() {
        super.onCreate()

        logger.debug("Migration service started")

        showForegroundNotification()

        scope.launch {
            try {
                val results = migrate()
                store.dispatch(MigrationAction.Result(results))
            } finally {
                logger.debug("Stopping migration service")
                shutdown()
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private suspend fun migrate(): MigrationResults {
        val results = migrator.migrateAsync().await()

        logger.debug("Migration completed:")

        results.forEach { (migration, run) ->
            logger.debug("$migration, version=${run.version} success=${run.success}")
        }

        return results
    }

    override fun onDestroy() {
        super.onDestroy()

        scope.cancel()
    }

    private fun showForegroundNotification() {
        val channel = ensureChannelExists()

        val builder = NotificationCompat.Builder(this, channel)
            .setSmallIcon(R.drawable.mozac_support_migration_notification_icon)
            .setContentTitle(getString(R.string.mozac_support_migration_ongoing_notification_title))
            .setContentText(getString(R.string.mozac_support_migration_ongoing_notification_text))
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)

        val id = SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)
        startForeground(id, builder.build())
    }

    private fun shutdown() {
        store.dispatch(MigrationAction.Completed)

        stopForeground(true)

        // Now after the migration this notification channel is no longer needed and we can just
        // remove it again.
        removeChannel()

        stopSelf()
    }

    private fun ensureChannelExists(): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = getSystemService(
                Context.NOTIFICATION_SERVICE
            ) as NotificationManager

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                TEMPORARY_NOTIFICATION_CHANNEL_NAME,
                NotificationManager.IMPORTANCE_LOW
            )
            channel.setShowBadge(false)
            channel.lockscreenVisibility = NotificationCompat.VISIBILITY_PUBLIC

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    private fun removeChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = getSystemService(
                Context.NOTIFICATION_SERVICE
            ) as NotificationManager

            notificationManager.deleteNotificationChannel(NOTIFICATION_CHANNEL_ID)
        }
    }
}
