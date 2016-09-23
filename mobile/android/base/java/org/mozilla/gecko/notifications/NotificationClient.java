/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoService;
import org.mozilla.gecko.NotificationListener;

/**
 * Client for posting notifications through a NotificationHandler.
 */
public abstract class NotificationClient implements NotificationListener {
    private static final String LOGTAG = "GeckoNotificationClient";

    private volatile NotificationHandler mHandler;
    private boolean mReady;
    private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();

    @Override // NotificationListener
    public void showNotification(String name, String cookie, String title,
                                 String text, String host, String imageUrl)
    {
        // The intent to launch when the user clicks the expanded notification
        final Intent notificationIntent = new Intent(GeckoApp.ACTION_ALERT_CALLBACK);
        notificationIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                        AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        notificationIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Put the strings into the intent as an URI
        // "alert:?name=<name>&cookie=<cookie>"
        final Uri.Builder b = new Uri.Builder();
        final Uri dataUri = b.scheme("alert")
                .appendQueryParameter("name", name)
                .appendQueryParameter("cookie", cookie)
                .build();
        notificationIntent.setData(dataUri);

        final PendingIntent clickIntent = PendingIntent.getActivity(
                GeckoAppShell.getApplicationContext(), 0, notificationIntent,
                PendingIntent.FLAG_UPDATE_CURRENT);

        add(name, imageUrl, host, title, text, clickIntent, /* closeIntent */ null);
        GeckoAppShell.onNotificationShow(name);
    }

    @Override // NotificationListener
    public void showPersistentNotification(String name, String cookie, String title,
                                           String text, String host, String imageUrl,
                                           String data)
    {
        final Context context = GeckoAppShell.getApplicationContext();

        final PendingIntent clickIntent = PendingIntent.getService(
                context, 0, GeckoService.getIntentToCreateServices(
                    context, "persistent-notification-click", data),
                PendingIntent.FLAG_UPDATE_CURRENT);

        final PendingIntent closeIntent = PendingIntent.getService(
                context, 0, GeckoService.getIntentToCreateServices(
                    context, "persistent-notification-close", data),
                PendingIntent.FLAG_UPDATE_CURRENT);

        add(name, imageUrl, host, title, text, clickIntent, closeIntent);
        GeckoAppShell.onNotificationShow(name);
    }

    @Override
    public void closeNotification(String name)
    {
        remove(name);
        GeckoAppShell.onNotificationClose(name);
    }

    public void onNotificationClick(String name) {
        GeckoAppShell.onNotificationClick(name);

        if (isOngoing(name)) {
            // When clicked, keep the notification if it displays progress
            return;
        }

        closeNotification(name);
    }

    /**
     * Adds a notification.
     *
     * @see NotificationHandler#add(String, String, String, String, String, PendingIntent, PendingIntent)
     */
    public synchronized void add(final String aName, final String aImageUrl, final String aHost,
                                 final String aAlertTitle, final String aAlertText,
                                 final PendingIntent contentIntent, final PendingIntent deleteIntent) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.add(aName, aImageUrl, aHost, aAlertTitle,
                             aAlertText, contentIntent, deleteIntent);
            }
        });
        notify();

        if (!mReady) {
            bind();
        }
    }

    /**
     * Adds a notification.
     *
     * @see NotificationHandler#add(String, Notification)
     */
    public synchronized void add(final String name, final Notification notification) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.add(name, notification);
            }
        });
        notify();

        if (!mReady) {
            bind();
        }
    }

    /**
     * Removes a notification.
     *
     * @see NotificationHandler#remove(String)
     */
    public synchronized void remove(final String name) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.remove(name);
            }
        });

        // If mReady == false, we haven't added any notifications yet. That can happen if Fennec is being
        // started in response to clicking a notification. Call bind() to ensure the task we posted above is run.
        if (!mReady) {
            bind();
        }

        notify();
    }

    /**
     * Determines whether a notification is showing progress.
     *
     * @see NotificationHandler#isOngoing(int)
     */
    public boolean isOngoing(String name) {
        final NotificationHandler handler = mHandler;
        return handler != null && handler.isOngoing(name);
    }

    protected void bind() {
        mReady = true;
    }

    protected void unbind() {
        mReady = false;
    }

    protected void connectHandler(NotificationHandler handler) {
        mHandler = handler;
        new Thread(new NotificationRunnable()).start();
    }

    private class NotificationRunnable implements Runnable {
        @Override
        public void run() {
            Runnable r;
            try {
                while (true) {
                    // Synchronize polls to prevent tasks from being added to the queue
                    // during the isDone check.
                    synchronized (NotificationClient.this) {
                        r = mTaskQueue.poll();
                        while (r == null) {
                            if (mHandler.isDone()) {
                                unbind();
                                return;
                            }
                            NotificationClient.this.wait();
                            r = mTaskQueue.poll();
                        }
                    }
                    r.run();
                }
            } catch (InterruptedException e) {
                Log.e(LOGTAG, "Notification task queue processing interrupted", e);
            }
        }
    }
}
