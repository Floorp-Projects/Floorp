/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Intent;
import android.util.Log;

import java.util.concurrent.SynchronousQueue;

class FilePickerResultHandlerSync extends FilePickerResultHandler {
    private static final String LOGTAG = "GeckoFilePickerResultHandlerSync";

    FilePickerResultHandlerSync(SynchronousQueue<String> resultQueue) {
        super(resultQueue);
    }

    public void onActivityResult(int resultCode, Intent data) {
        try {
            mFilePickerResult.put(handleActivityResult(resultCode, data));
        } catch (InterruptedException e) {
            Log.i(LOGTAG, "error returning file picker result", e);
        }
    }
}
