/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;

/**
 * Client for posting notifications through the NotificationService.
 */
public class ServiceNotificationClient extends NotificationClient {
    private static final String LOGTAG = "GeckoServiceNotificationClient";

    private final ServiceConnection mConnection = new NotificationServiceConnection();
    private boolean mBound;
    private final Context mContext;

    public ServiceNotificationClient(Context context) {
        mContext = context;
    }

    @Override
    protected void bind() {
        super.bind();
        final Intent intent = new Intent(mContext, NotificationService.class);
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void unbind() {
        // If there are no more tasks and no notifications being
        // displayed, the service is disconnected. Unfortunately,
        // since completed download notifications are shown by
        // removing the progress notification and creating a new
        // static one, this will cause the service to be unbound
        // and immediately rebound when a download completes.
        super.unbind();

        if (mBound) {
            mBound = false;
            mContext.unbindService(mConnection);
        }
    }

    class NotificationServiceConnection implements ServiceConnection {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            final NotificationService.NotificationBinder binder =
                    (NotificationService.NotificationBinder) service;
            connectHandler(binder.getService().getNotificationHandler());
            mBound = true;
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
    }
}
