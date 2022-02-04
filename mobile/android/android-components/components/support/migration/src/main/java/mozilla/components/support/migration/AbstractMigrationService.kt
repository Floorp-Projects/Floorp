/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationStore
import mozilla.components.support.utils.PendingIntentUtils

private const val NOTIFICATION_CHANNEL_ID = "mozac.support.migration.generic"

private const val NOTIFICATION_TAG = "mozac.support.migration.notification"
private const val NOTIFICATION_COMPLETED_ID = 1

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
    protected abstract val migrationDecisionActivity: Class<out Activity>

    private val logger = Logger("MigrationService")
    private val scope = MainScope()

    override fun onCreate() {
        super.onCreate()

        logger.debug("Migration service started")

        showMigrationStartedNotification()

        scope.launch {
            try {
                migrate()
            } finally {
                logger.debug("Stopping migration service")
                shutdown()
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private suspend fun migrate(): MigrationResults {
        val results = migrator.migrateAsync(store).await()

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

    private fun shutdown() {
        store.dispatch(MigrationAction.Completed)

        emitCompletedFact()

        stopForeground(true)

        showMigrationCompleteNotification()

        stopSelf()
    }

    private fun showMigrationStartedNotification() {
        val channel: String = ensureChannelExists()
        val titleRes: Int = R.string.mozac_support_migration_ongoing_notification_title
        val contentRes: Int = R.string.mozac_support_migration_ongoing_notification_text
        val builder = getNotificationBuilder(titleRes, contentRes, channel)
        val id = SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)

        startForeground(id, builder.build())
    }

    private fun showMigrationCompleteNotification() {
        val channel: String = NOTIFICATION_CHANNEL_ID
        val titleRes: Int = R.string.mozac_support_migration_complete_notification_title
        val contentRes: Int = R.string.mozac_support_migration_complete_notification_text
        val builder = getNotificationBuilder(titleRes, contentRes, channel).apply {
            setAutoCancel(true)
        }

        NotificationManagerCompat.from(this).notify(
            NOTIFICATION_TAG,
            NOTIFICATION_COMPLETED_ID,
            builder.build()
        )
    }

    private fun getNotificationBuilder(titleRes: Int, contentRes: Int, channel: String): NotificationCompat.Builder {
        return NotificationCompat.Builder(this, channel)
            .setSmallIcon(R.drawable.mozac_support_migration_notification_icon)
            .setContentTitle(getString(titleRes))
            .setContentText(getString(contentRes))
            .setContentIntent(
                PendingIntent.getActivity(
                    this,
                    0,
                    Intent(this, migrationDecisionActivity).setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP),
                    PendingIntentUtils.defaultFlags
                )
            )
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
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

    companion object {
        /**
         * Dismisses the "migration completed" notification.
         */
        fun dismissNotification(context: Context) {
            NotificationManagerCompat.from(context).cancel(
                NOTIFICATION_TAG,
                NOTIFICATION_COMPLETED_ID
            )
        }
    }
}
