/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoThread.FileDescriptors;
import org.mozilla.gecko.GeckoThread.ParcelFileDescriptors;
import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.ICompositorSurfaceManager;
import org.mozilla.gecko.gfx.ISurfaceAllocator;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoServiceChildProcess extends Service {
  private static final String LOGTAG = "ServiceChildProcess";

  private static IProcessManager sProcessManager;
  private static String sOwnerProcessId;
  private final MemoryController mMemoryController = new MemoryController();

  // Makes sure we don't reuse this process
  private static boolean sCreateCalled;

  @WrapForJNI(calledFrom = "gecko")
  private static void getEditableParent(
      final IGeckoEditableChild child, final long contentId, final long tabId) {
    try {
      sProcessManager.getEditableParent(child, contentId, tabId);
    } catch (final RemoteException e) {
      Log.e(LOGTAG, "Cannot get editable", e);
    }
  }

  @Override
  public void onCreate() {
    super.onCreate();
    Log.i(LOGTAG, "onCreate");

    if (sCreateCalled) {
      // We don't support reusing processes, and this could get us in a really weird state,
      // so let's throw here.
      throw new RuntimeException("Cannot reuse process.");
    }
    sCreateCalled = true;

    GeckoAppShell.setApplicationContext(getApplicationContext());
    GeckoThread.launch(); // Preload Gecko.
  }

  protected static class ChildProcessBinder extends IChildProcess.Stub {
    @Override
    public int getPid() {
      return Process.myPid();
    }

    @Override
    public int start(
        final IProcessManager procMan,
        final String mainProcessId,
        final String[] args,
        final Bundle extras,
        final int flags,
        final String userSerialNumber,
        final String crashHandlerService,
        final ParcelFileDescriptor prefsPfd,
        final ParcelFileDescriptor prefMapPfd,
        final ParcelFileDescriptor ipcPfd,
        final ParcelFileDescriptor crashReporterPfd,
        final ParcelFileDescriptor crashAnnotationPfd) {

      final ParcelFileDescriptors pfds =
          ParcelFileDescriptors.builder()
              .prefs(prefsPfd)
              .prefMap(prefMapPfd)
              .ipc(ipcPfd)
              .crashReporter(crashReporterPfd)
              .crashAnnotation(crashAnnotationPfd)
              .build();

      synchronized (GeckoServiceChildProcess.class) {
        if (sOwnerProcessId != null && !sOwnerProcessId.equals(mainProcessId)) {
          Log.w(
              LOGTAG,
              "This process belongs to a different GeckoRuntime owner: "
                  + sOwnerProcessId
                  + " process: "
                  + mainProcessId);
          // We need to close the File Descriptors here otherwise we will leak them causing a
          // shutdown hang.
          pfds.close();
          return IChildProcess.STARTED_BUSY;
        }
        if (sProcessManager != null) {
          Log.e(LOGTAG, "Child process already started");
          pfds.close();
          return IChildProcess.STARTED_FAIL;
        }
        sProcessManager = procMan;
        sOwnerProcessId = mainProcessId;
      }

      final FileDescriptors fds = pfds.detach();
      ThreadUtils.runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              if (crashHandlerService != null) {
                try {
                  @SuppressWarnings("unchecked")
                  final Class<? extends Service> crashHandler =
                      (Class<? extends Service>) Class.forName(crashHandlerService);

                  // Native crashes are reported through pipes, so we don't have to
                  // do anything special for that.
                  GeckoAppShell.setCrashHandlerService(crashHandler);
                  GeckoAppShell.ensureCrashHandling(crashHandler);
                } catch (final ClassNotFoundException e) {
                  Log.w(LOGTAG, "Couldn't find crash handler service " + crashHandlerService);
                }
              }

              final GeckoThread.InitInfo info =
                  GeckoThread.InitInfo.builder()
                      .args(args)
                      .extras(extras)
                      .flags(flags)
                      .userSerialNumber(userSerialNumber)
                      .fds(fds)
                      .build();

              if (GeckoThread.init(info)) {
                GeckoThread.launch();
              }
            }
          });
      return IChildProcess.STARTED_OK;
    }

    @Override
    public void crash() {
      GeckoThread.crash();
    }

    @Override
    public ICompositorSurfaceManager getCompositorSurfaceManager() {
      Log.e(
          LOGTAG, "Invalid call to IChildProcess.getCompositorSurfaceManager for non-GPU process");
      throw new AssertionError(
          "Invalid call to IChildProcess.getCompositorSurfaceManager for non-GPU process.");
    }

    @Override
    public ISurfaceAllocator getSurfaceAllocator(final int allocatorId) {
      Log.e(LOGTAG, "Invalid call to IChildProcess.getSurfaceAllocator for non-GPU process");
      throw new AssertionError(
          "Invalid call to IChildProcess.getSurfaceAllocator for non-GPU process.");
    }
  }

  protected Binder createBinder() {
    return new ChildProcessBinder();
  }

  private final Binder mBinder = createBinder();

  @Override
  public void onDestroy() {
    Log.i(LOGTAG, "Destroying GeckoServiceChildProcess");
    System.exit(0);
  }

  @Override
  public IBinder onBind(final Intent intent) {
    // Calling stopSelf ensures that whenever the client unbinds the process dies immediately.
    stopSelf();
    return mBinder;
  }

  @Override
  public void onTrimMemory(final int level) {
    mMemoryController.onTrimMemory(level);

    // This is currently a no-op in Service, but let's future-proof.
    super.onTrimMemory(level);
  }

  @Override
  public void onLowMemory() {
    mMemoryController.onLowMemory();
    super.onLowMemory();
  }

  /**
   * Returns the surface allocator interface that should be used by this process to allocate
   * Surfaces, for consumption in either the GPU process or parent process.
   */
  public static ISurfaceAllocator getSurfaceAllocator() throws RemoteException {
    return sProcessManager.getSurfaceAllocator();
  }
}
