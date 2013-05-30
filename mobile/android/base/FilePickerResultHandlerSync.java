/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Intent;
import android.util.Log;

import java.util.Queue;

class FilePickerResultHandlerSync extends FilePickerResultHandler {
    private static final String LOGTAG = "GeckoFilePickerResultHandlerSync";

    FilePickerResultHandlerSync(Queue<String> resultQueue) {
        super(resultQueue, null);
    }

    /* Use this constructor to asynchronously listen for results */
    public FilePickerResultHandlerSync(ActivityHandlerHelper.FileResultHandler handler) {
        super(null, handler);
    }

    @Override
    public void onActivityResult(int resultCode, Intent data) {
        if (mFilePickerResult != null)
            mFilePickerResult.offer(handleActivityResult(resultCode, data));

        if (mHandler != null)
            mHandler.gotFile(handleActivityResult(resultCode, data));
    }
}
