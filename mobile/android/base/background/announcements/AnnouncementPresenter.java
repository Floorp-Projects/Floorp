/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.net.URI;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.GlobalConstants;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

/**
 * Handle requests to display a fetched announcement.
 */
public class AnnouncementPresenter {

  /**
   * Display the provided snippet.
   * @param context
   *        The context instance to use when obtaining the NotificationManager.
   * @param notificationID
   *        A unique ID for this notification.
   * @param title
   *        The *already localized* String title. Must not be null.
   * @param body
   *        The *already localized* String body. Must not be null.
   * @param uri
   *        The URL to open when the notification is tapped.
   */
  @SuppressWarnings("deprecation")
  public static void displayAnnouncement(final Context context,
                                         final int notificationID,
                                         final String title,
                                         final String body,
                                         final URI uri) {
    final String ns = Context.NOTIFICATION_SERVICE;
    final NotificationManager notificationManager = (NotificationManager) context.getSystemService(ns);

    // Set pending intent associated with the notification.
    Uri u = Uri.parse(uri.toASCIIString());
    Intent intent = new Intent(Intent.ACTION_VIEW, u);

    // Always open the link with Fennec.
    intent.setClassName(GlobalConstants.BROWSER_INTENT_PACKAGE, GlobalConstants.BROWSER_INTENT_CLASS);
    PendingIntent contentIntent = PendingIntent.getActivity(context, 0, intent, 0);

    final int icon = R.drawable.ic_status_logo;

    // Deprecated approach to building a notification.
    final long when = System.currentTimeMillis();
    Notification notification = new Notification(icon, title, when);
    notification.flags = Notification.FLAG_AUTO_CANCEL;
    notification.setLatestEventInfo(context, title, body, contentIntent);

    // Notification.Builder since API 11.
    /*
    Notification notification = new Notification.Builder(context)
        .setContentTitle(title)
        .setContentText(body)
        .setAutoCancel(true)
        .setContentIntent(contentIntent).getNotification();
     */

    // Send notification.
    notificationManager.notify(notificationID, notification);
  }

  public static void displayAnnouncement(final Context context,
                                         final Announcement snippet) {
    final int notificationID = snippet.getId();
    final String title = snippet.getTitle();
    final String body = snippet.getText();
    final URI uri = snippet.getUri();
    AnnouncementPresenter.displayAnnouncement(context, notificationID, title, body, uri);
  }
}
