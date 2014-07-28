/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Notification;
import android.app.PendingIntent;
import android.text.TextUtils;
import android.util.Log;

import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Client for posting notifications through a NotificationHandler.
 */
public abstract class NotificationClient {
    private static final String LOGTAG = "GeckoNotificationClient";

    private volatile NotificationHandler mHandler;
    private boolean mReady;
    private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();
    private final ConcurrentHashMap<Integer, UpdateRunnable> mUpdatesMap =
            new ConcurrentHashMap<Integer, UpdateRunnable>();

    /**
     * Runnable that is reused between update notifications.
     *
     * Updates happen frequently, so reusing Runnables prevents frequent dynamic allocation.
     */
    private class UpdateRunnable implements Runnable {
        private long mProgress;
        private long mProgressMax;
        private String mAlertText;
        final private int mNotificationID;

        public UpdateRunnable(int notificationID) {
            mNotificationID = notificationID;
        }

        public synchronized boolean updateProgress(long progress, long progressMax, String alertText) {
            if (progress == mProgress
                    && mProgressMax == progressMax
                    && TextUtils.equals(mAlertText, alertText)) {
                return false;
            }

            mProgress = progress;
            mProgressMax = progressMax;
            mAlertText = alertText;
            return true;
        }

        @Override
        public void run() {
            long progress;
            long progressMax;
            String alertText;

            synchronized (this) {
                progress = mProgress;
                progressMax = mProgressMax;
                alertText = mAlertText;
            }

            mHandler.update(mNotificationID, progress, progressMax, alertText);
        }
    };

    /**
     * Adds a notification.
     *
     * @see NotificationHandler#add(int, String, String, String, PendingIntent, PendingIntent)
     */
    public synchronized void add(final int notificationID, final String aImageUrl,
            final String aAlertTitle, final String aAlertText, final PendingIntent contentIntent) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.add(notificationID, aImageUrl, aAlertTitle, aAlertText, contentIntent);
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
     * @see NotificationHandler#add(int, Notification)
     */
    public synchronized void add(final int notificationID, final Notification notification) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.add(notificationID, notification);
            }
        });
        notify();

        if (!mReady) {
            bind();
        }
    }

    /**
     * Updates a notification.
     *
     * @see NotificationHandler#update(int, long, long, String)
     */
    public void update(final int notificationID, final long aProgress, final long aProgressMax,
            final String aAlertText) {
        UpdateRunnable runnable = mUpdatesMap.get(notificationID);

        if (runnable == null) {
            runnable = new UpdateRunnable(notificationID);
            mUpdatesMap.put(notificationID, runnable);
        }

        // If we've already posted an update with these values, there's no
        // need to do it again.
        if (!runnable.updateProgress(aProgress, aProgressMax, aAlertText)) {
            return;
        }

        synchronized (this) {
            if (mReady) {
                mTaskQueue.add(runnable);
                notify();
            }
        }
    }

    /**
     * Removes a notification.
     *
     * @see NotificationHandler#remove(int)
     */
    public synchronized void remove(final int notificationID) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mHandler.remove(notificationID);
                mUpdatesMap.remove(notificationID);
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
     * @see NotificationHandler#isProgressStyle(int)
     */
    public boolean isOngoing(int notificationID) {
        final NotificationHandler handler = mHandler;
        return handler != null && handler.isOngoing(notificationID);
    }

    protected void bind() {
        mReady = true;
    }

    protected void unbind() {
        mReady = false;
        mUpdatesMap.clear();
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
