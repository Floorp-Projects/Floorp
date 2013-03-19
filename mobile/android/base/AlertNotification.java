/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.util.Log;
import android.widget.RemoteViews;

import java.net.URL;
import java.text.NumberFormat;

public class AlertNotification
    extends Notification
{
    private static final String LOGTAG = "GeckoAlertNotification";

    private final int mId;
    private final int mIcon;
    private final String mTitle;
    private final String mText;
    private final NotificationManager mNotificationManager;

    private boolean mProgressStyle;
    private double mPrevPercent  = -1;
    private String mPrevAlertText = "";

    private static final double UPDATE_THRESHOLD = .01;
    private Context mContext;

    public AlertNotification(Context aContext, int aNotificationId, int aIcon,
                             String aTitle, String aText, long aWhen, Uri aIconUri) {
        super(aIcon, (aText.length() > 0) ? aText : aTitle, aWhen);

        mIcon = aIcon;
        mTitle = aTitle;
        mText = aText;
        mId = aNotificationId;
        mContext = aContext;

        mNotificationManager = (NotificationManager) aContext.getSystemService(Context.NOTIFICATION_SERVICE);

        if (aIconUri == null || aIconUri.getScheme() == null)
            return;

        // Custom view
        int layout = R.layout.notification_icon_text;
        RemoteViews view = new RemoteViews(mContext.getPackageName(), layout);
        try {
            URL url = new URL(aIconUri.toString());
            Bitmap bm = BitmapFactory.decodeStream(url.openStream());
            if (bm == null) {
                Log.e(LOGTAG, "failed to decode icon");
                return;
            }
            view.setImageViewBitmap(R.id.notification_image, bm);
            view.setTextViewText(R.id.notification_title, mTitle);
            if (mText.length() > 0) {
                view.setTextViewText(R.id.notification_text, mText);
            }
            contentView = view;
        } catch (Exception e) {
            Log.e(LOGTAG, "failed to create bitmap", e);
        }
    }

    public int getId() {
        return mId;
    }

    public synchronized boolean isProgressStyle() {
        return mProgressStyle;
    }

    public void show() {
        mNotificationManager.notify(mId, this);
    }

    public void cancel() {
        mNotificationManager.cancel(mId);
    }

    public synchronized void updateProgress(String aAlertText, long aProgress, long aProgressMax) {
        if (!mProgressStyle) {
            // Custom view
            int layout =  aAlertText.length() > 0 ? R.layout.notification_progress_text : R.layout.notification_progress;

            RemoteViews view = new RemoteViews(mContext.getPackageName(), layout);
            view.setImageViewResource(R.id.notification_image, mIcon);
            view.setTextViewText(R.id.notification_title, mTitle);
            contentView = view;
            flags |= FLAG_ONGOING_EVENT | FLAG_ONLY_ALERT_ONCE;

            mProgressStyle = true;
        }

        String text;
        double percent = 0;
        if (aProgressMax > 0)
            percent = ((double)aProgress / (double)aProgressMax);

        if (aAlertText.length() > 0)
            text = aAlertText;
        else
            text = NumberFormat.getPercentInstance().format(percent); 

        if (mPrevAlertText.equals(text) && Math.abs(mPrevPercent - percent) < UPDATE_THRESHOLD)
            return;

        contentView.setTextViewText(R.id.notification_text, text);
        contentView.setProgressBar(R.id.notification_progressbar, (int)aProgressMax, (int)aProgress, false);

        // Update the notification
        mNotificationManager.notify(mId, this);

        mPrevPercent = percent;
        mPrevAlertText = text;
    }
}
