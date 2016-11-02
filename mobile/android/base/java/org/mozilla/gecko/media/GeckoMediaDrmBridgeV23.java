/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.annotation.TargetApi;
import static android.os.Build.VERSION_CODES.M;
import android.media.MediaDrm;
import android.util.Log;
import java.util.List;

public class GeckoMediaDrmBridgeV23 extends GeckoMediaDrmBridgeV21 {

    private static final String LOGTAG = "GeckoMediaDrmBridgeV23";
    private static final boolean DEBUG = false;

    GeckoMediaDrmBridgeV23(String keySystem) throws Exception {
        super(keySystem);
        if (DEBUG) Log.d(LOGTAG, "GeckoMediaDrmBridgeV23 ctor");
        mDrm.setOnKeyStatusChangeListener(new KeyStatusChangeListener(), null);
    }

    @TargetApi(M)
    private class KeyStatusChangeListener implements MediaDrm.OnKeyStatusChangeListener {
        @Override
        public void onKeyStatusChange(MediaDrm mediaDrm,
                                      byte[] sessionId,
                                      List<MediaDrm.KeyStatus> keyInformation,
                                      boolean hasNewUsableKey) {
            if (DEBUG) Log.d(LOGTAG, "[onKeyStatusChange] hasNewUsableKey = " + hasNewUsableKey);
            if (keyInformation.size() == 0) {
                return;
            }
            SessionKeyInfo[] keyInfos = new SessionKeyInfo[keyInformation.size()];
            for (int i = 0; i < keyInformation.size(); i++) {
                MediaDrm.KeyStatus keyStatus = keyInformation.get(i);
                keyInfos[i] = new SessionKeyInfo(keyStatus.getKeyId(),
                                                 keyStatus.getStatusCode());
            }
            onSessionBatchedKeyChanged(sessionId, keyInfos);
        }
    }
}
