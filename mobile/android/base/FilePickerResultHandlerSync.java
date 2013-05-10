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
        super(resultQueue);
    }

    @Override
    public void onActivityResult(int resultCode, Intent data) {
        mFilePickerResult.offer(handleActivityResult(resultCode, data));
    }
}
