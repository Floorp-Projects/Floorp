/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

public class GeckoServiceChildProcess extends Service {

    static private String LOGTAG = "GeckoServiceChildProcess";

    private static IProcessManager sProcessManager;

    static private void stop() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Process.killProcess(Process.myPid());
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko")
    private static IGeckoEditableParent getEditableParent(final long contentId,
                                                          final long tabId) {
        try {
            return sProcessManager.getEditableParent(contentId, tabId);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Cannot get editable", e);
            return null;
        }
    }

    public void onCreate() {
        super.onCreate();
    }

    public void onDestroy() {
        super.onDestroy();
    }

    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        return Service.START_STICKY;
    }

    private Binder mBinder = new IChildProcess.Stub() {
        @Override
        public void stop() {
            GeckoServiceChildProcess.stop();
        }

        @Override
        public int getPid() {
            return Process.myPid();
        }

        @Override
        public void start(final IProcessManager procMan,
                          final String[] args,
                          final ParcelFileDescriptor crashReporterPfd,
                          final ParcelFileDescriptor ipcPfd) {
            if (sProcessManager != null) {
                Log.e(LOGTAG, "Attempting to start a service that has already been started.");
                return;
            }
            sProcessManager = procMan;

            final int crashReporterFd = crashReporterPfd != null ? crashReporterPfd.detachFd() : -1;
            final int ipcFd = ipcPfd != null ? ipcPfd.detachFd() : -1;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    GeckoAppShell.ensureCrashHandling();
                    GeckoAppShell.setApplicationContext(getApplicationContext());
                    if (GeckoThread.initChildProcess(args, crashReporterFd, ipcFd)) {
                        GeckoThread.launch();
                    }
                }
            });
        }
    };

    public IBinder onBind(final Intent intent) {
        GeckoLoader.setLastIntent(new SafeIntent(intent));
        return mBinder;
    }

    public boolean onUnbind(Intent intent) {
        Log.i(LOGTAG, "Service has been unbound. Stopping.");
        stop();
        return false;
    }

    @JNITarget
    static public final class geckomediaplugin extends GeckoServiceChildProcess {}

    @JNITarget
    static public final class tab extends GeckoServiceChildProcess {}
}
