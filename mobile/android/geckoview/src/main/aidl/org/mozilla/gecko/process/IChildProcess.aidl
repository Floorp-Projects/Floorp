/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;
import android.os.ParcelFileDescriptor;

interface IChildProcess {
    void stop();
    int getPid();
    void start(in String[] args, in ParcelFileDescriptor crashReporterPfd, in ParcelFileDescriptor ipcPfd);
}
