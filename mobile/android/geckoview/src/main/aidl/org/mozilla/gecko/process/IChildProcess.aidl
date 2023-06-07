/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.gfx.ICompositorSurfaceManager;
import org.mozilla.gecko.gfx.ISurfaceAllocator;
import org.mozilla.gecko.process.IProcessManager;

import android.os.Bundle;
import android.os.ParcelFileDescriptor;

interface IChildProcess {
    /** The process started correctly. */
    const int STARTED_OK = 0;
    /** An error occurred when trying to start this process. */
    const int STARTED_FAIL = 1;
    /** This process is being used elsewhere and cannot start. */
    const int STARTED_BUSY = 2;

    int getPid();
    int start(in IProcessManager procMan,
              in String mainProcessId,
              in String[] args,
              in Bundle extras,
              int flags,
              in String userSerialNumber,
              in String crashHandlerService,
              in ParcelFileDescriptor prefsPfd,
              in ParcelFileDescriptor prefMapPfd,
              in ParcelFileDescriptor ipcPfd,
              in ParcelFileDescriptor crashReporterPfd);

    void crash();

    /** Must only be called for a GPU child process type. */
    ICompositorSurfaceManager getCompositorSurfaceManager();

    /**
     * Returns the interface that other processes should use to allocate Surfaces to be
     * consumed by the GPU process. Must only be called for a GPU child process type.
     * @param allocatorId A unique ID used to identify the GPU process instance the allocator
     *     belongs to.
     */
    ISurfaceAllocator getSurfaceAllocator(int allocatorId);
}
