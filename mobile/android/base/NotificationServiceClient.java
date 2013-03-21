/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.text.TextUtils;
import android.util.Log;

/**
 * Client for posting notifications through the NotificationService.
 */
public class NotificationServiceClient {
    private static final String LOGTAG = "GeckoNotificationServiceClient";

    private volatile NotificationService mService;
    private final ServiceConnection mConnection = new NotificationServiceConnection();
    private boolean mBound;
    private final Context mContext;
    private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();
    private final ConcurrentHashMap<Integer, UpdateRunnable> mUpdatesMap =
            new ConcurrentHashMap<Integer, UpdateRunnable>();

    public NotificationServiceClient(Context context) {
        mContext = context;
    }

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

            mService.update(mNotificationID, progress, progressMax, alertText);
        }
    };

    /**
     * Adds a notification.
     *
     * @see NotificationService#add(int, String, String, String, PendingIntent, PendingIntent)
     */
    public synchronized void add(final int notificationID, final String aImageUrl,
            final String aAlertTitle, final String aAlertText, final PendingIntent contentIntent) {
        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mService.add(notificationID, aImageUrl, aAlertTitle, aAlertText, contentIntent);
            }
        });
        notify();

        if (!mBound) {
            bind();
        }
    }

    /**
     * Updates a notification.
     *
     * @see NotificationService#update(int, long, long, String)
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
            if (mBound) {
                mTaskQueue.add(runnable);
                notify();
            }
        }
    }

    /**
     * Removes a notification.
     *
     * @see NotificationService#remove(int)
     */
    public synchronized void remove(final int notificationID) {
        if (!mBound) {
            return;
        }

        mTaskQueue.add(new Runnable() {
            @Override
            public void run() {
                mService.remove(notificationID);
                mUpdatesMap.remove(notificationID);
            }
        });
        notify();
    }

    /**
     * Determines whether a notification is showing progress.
     *
     * @see NotificationService#isProgressStyle(int)
     */
    public boolean isProgressStyle(int notificationID) {
        final NotificationService service = mService;
        return service != null && service.isProgressStyle(notificationID);
    }

    private void bind() {
        mBound = true;
        final Intent intent = new Intent(mContext, NotificationService.class);
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    private void unbind() {
        if (mBound) {
            mBound = false;
            mContext.unbindService(mConnection);
            mUpdatesMap.clear();
        }
    }

    class NotificationServiceConnection implements ServiceConnection, Runnable {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            final NotificationService.NotificationBinder binder =
                    (NotificationService.NotificationBinder) service;
            mService = binder.getService();

            new Thread(this).start();
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            // This is called when the connection with the service has been
            // unexpectedly disconnected -- that is, its process crashed.
            // Because it is running in our same process, we should never
            // see this happen, and the correctness of this class relies on
            // this never happening.
            Log.e(LOGTAG, "Notification service disconnected", new Exception());
        }

        @Override
        public void run() {
            Runnable r;
            try {
                while (true) {
                    // Synchronize polls to prevent tasks from being added to the queue
                    // during the isDone check.
                    synchronized (NotificationServiceClient.this) {
                        r = mTaskQueue.poll();
                        while (r == null) {
                            if (mService.isDone()) {
                                // If there are no more tasks and no notifications being
                                // displayed, the service is disconnected. Unfortunately,
                                // since completed download notifications are shown by
                                // removing the progress notification and creating a new
                                // static one, this will cause the service to be unbound
                                // and immediately rebound when a download completes.
                                unbind();
                                return;
                            }
                            NotificationServiceClient.this.wait();
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
