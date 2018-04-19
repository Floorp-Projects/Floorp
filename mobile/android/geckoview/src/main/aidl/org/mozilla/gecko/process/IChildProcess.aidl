/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.process.IProcessManager;

import android.os.Bundle;
import android.os.ParcelFileDescriptor;

interface IChildProcess {
    int getPid();
    boolean start(in IProcessManager procMan, in String[] args, in Bundle extras, int flags,
                  in ParcelFileDescriptor prefsPfd, in ParcelFileDescriptor ipcPfd,
                  in ParcelFileDescriptor crashReporterPfd,
                  in ParcelFileDescriptor crashAnnotationPfd);
}
