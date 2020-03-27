/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

public class GeckoServiceChildProcess extends Service {
    private static final String LOGTAG = "ServiceChildProcess";
    private static IProcessManager sProcessManager;

    @WrapForJNI(calledFrom = "gecko")
    private static void getEditableParent(final IGeckoEditableChild child,
                                          final long contentId,
                                          final long tabId) {
        try {
            sProcessManager.getEditableParent(child, contentId, tabId);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Cannot get editable", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();

        GeckoAppShell.setApplicationContext(getApplicationContext());
    }

    @Override
    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        return Service.START_NOT_STICKY;
    }

    private final Binder mBinder = new IChildProcess.Stub() {
        @Override
        public int getPid() {
            return Process.myPid();
        }

        @Override
        public boolean start(final IProcessManager procMan,
                             final String[] args,
                             final Bundle extras,
                             final int flags,
                             final String crashHandlerService,
                             final ParcelFileDescriptor prefsPfd,
                             final ParcelFileDescriptor prefMapPfd,
                             final ParcelFileDescriptor ipcPfd,
                             final ParcelFileDescriptor crashReporterPfd,
                             final ParcelFileDescriptor crashAnnotationPfd) {
            synchronized (GeckoServiceChildProcess.class) {
                if (sProcessManager != null) {
                    Log.e(LOGTAG, "Child process already started");
                    return false;
                }
                sProcessManager = procMan;
            }

            final int prefsFd = prefsPfd != null ?
                                prefsPfd.detachFd() : -1;
            final int prefMapFd = prefMapPfd != null ?
                                  prefMapPfd.detachFd() : -1;
            final int ipcFd = ipcPfd.detachFd();
            final int crashReporterFd = crashReporterPfd != null ?
                                        crashReporterPfd.detachFd() : -1;
            final int crashAnnotationFd = crashAnnotationPfd != null ?
                                          crashAnnotationPfd.detachFd() : -1;

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    if (crashHandlerService != null) {
                        try {
                            @SuppressWarnings("unchecked")
                            final Class<? extends Service> crashHandler = (Class<? extends Service>) Class.forName(crashHandlerService);

                            // Native crashes are reported through pipes, so we don't have to
                            // do anything special for that.
                            GeckoAppShell.setCrashHandlerService(crashHandler);
                            GeckoAppShell.ensureCrashHandling(crashHandler);
                        } catch (ClassNotFoundException e) {
                            Log.w(LOGTAG, "Couldn't find crash handler service " + crashHandlerService);
                        }
                    }

                    final GeckoThread.InitInfo info = new GeckoThread.InitInfo();
                    info.args = args;
                    info.extras = extras;
                    info.flags = flags;
                    info.prefsFd = prefsFd;
                    info.prefMapFd = prefMapFd;
                    info.ipcFd = ipcFd;
                    info.crashFd = crashReporterFd;
                    info.crashAnnotationFd = crashAnnotationFd;

                    if (GeckoThread.init(info)) {
                        GeckoThread.launch();
                    }
                }
            });
            return true;
        }

        @Override
        public void crash() {
            GeckoThread.crash();
        }
    };

    @Override
    public IBinder onBind(final Intent intent) {
        GeckoThread.launch(); // Preload Gecko.
        return mBinder;
    }

    @Override
    public boolean onUnbind(final Intent intent) {
        Log.i(LOGTAG, "Service has been unbound. Stopping.");
        stopSelf();
        Process.killProcess(Process.myPid());
        return false;
    }
}
