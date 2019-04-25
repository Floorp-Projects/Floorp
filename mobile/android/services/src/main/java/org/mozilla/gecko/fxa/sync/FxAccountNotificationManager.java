/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountWebFlowActivity;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.Action;
import org.mozilla.gecko.notifications.NotificationHelper;

/**
 * Abstraction that manages notifications shown or hidden for a Firefox Account.
 * <p>
 * In future, we anticipate this tracking things like:
 * <ul>
 * <li>new engines to offer to Sync;</li>
 * <li>service interruption updates;</li>
 * <li>messages from other clients.</li>
 * </ul>
 */
public class FxAccountNotificationManager {
  private static final String LOG_TAG = FxAccountNotificationManager.class.getSimpleName();

  protected final int notificationId;

  // We're lazy about updating our locale info, because most syncs don't notify.
  private volatile boolean localeUpdated;

  public FxAccountNotificationManager(int notificationId) {
    this.notificationId = notificationId;
  }

  /**
   * Remove all Firefox Account related notifications from the notification manager.
   *
   * @param context
   *          Android context.
   */
  public void clear(Context context) {
    final NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
    notificationManager.cancel(notificationId);
  }

  /**
   * Reflect new Firefox Account state to the notification manager: show or hide
   * notifications reflecting the state of a Firefox Account.
   *
   * @param context
   *          Android context.
   * @param fxAccount
   *          Firefox Account to reflect to the notification manager.
   */
  @SuppressWarnings("NewApi")
  public void update(Context context, AndroidFxAccount fxAccount) {
    final NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

    final State state = fxAccount.getState();
    final Action action = state.getNeededAction();
    if (action == Action.None) {
      Logger.info(LOG_TAG, "State " + state.getStateLabel() + " needs no action; cancelling any existing notification.");
      notificationManager.cancel(notificationId);
      return;
    }

    if (!localeUpdated) {
      localeUpdated = true;
      Locales.getLocaleManager().getAndApplyPersistedLocale(context);
    }

    final String title;
    final String text;
    final Intent notificationIntent;
    if (action == Action.NeedsFinishMigrating) {
      //TelemetryWrapper.addToHistogram(TelemetryContract.SYNC11_MIGRATION_NOTIFICATIONS_OFFERED, 1);

      title = context.getResources().getString(R.string.fxaccount_sync_finish_migrating_notification_title);
      text = context.getResources().getString(R.string.fxaccount_sync_finish_migrating_notification_text, state.email);
      notificationIntent = new Intent(FxAccountConstants.ACTION_FXA_FINISH_MIGRATING);
    } else {
      title = context.getResources().getString(R.string.fxaccount_sync_sign_in_error_notification_title);
      text = context.getResources().getString(R.string.fxaccount_sync_sign_in_error_notification_text, state.email);
      notificationIntent = new Intent(FxAccountConstants.ACTION_FXA_STATUS);
    }

    notificationIntent.putExtra(FxAccountWebFlowActivity.EXTRA_ENDPOINT, FxAccountConstants.ENDPOINT_NOTIFICATION);

    Logger.info(LOG_TAG, "State " + state.getStateLabel() + " needs action; offering notification with title: " + title);
    FxAccountUtils.pii(LOG_TAG, "And text: " + text);

    final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, notificationIntent, 0);

    final Notification.Builder builder = new Notification.Builder(context);
    builder
    .setContentTitle(title)
    .setContentText(text)
    .setSmallIcon(R.drawable.ic_status_logo)
    .setAutoCancel(true)
    .setContentIntent(pendingIntent);

    if (!AppConstants.Versions.preO) {
      builder.setChannelId(NotificationHelper.getInstance(context)
              .getNotificationChannel(NotificationHelper.Channel.DEFAULT).getId());
    }

    notificationManager.notify(notificationId, builder.build());
  }
}
