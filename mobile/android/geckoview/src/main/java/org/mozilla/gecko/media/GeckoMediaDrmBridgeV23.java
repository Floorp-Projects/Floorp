/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.annotation.TargetApi;
import android.media.DeniedByServerException;
import android.media.NotProvisionedException;

import static android.os.Build.VERSION_CODES.M;
import android.media.MediaDrm;
import android.util.Log;
import java.lang.IllegalStateException;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.List;

@TargetApi(M)
public class GeckoMediaDrmBridgeV23 extends GeckoMediaDrmBridgeV21 {
    private static final boolean DEBUG = false;

    GeckoMediaDrmBridgeV23(String keySystem) throws Exception {
        super(keySystem);
        if (DEBUG) Log.d(LOGTAG, "GeckoMediaDrmBridgeV23 ctor");
        mDrm.setOnKeyStatusChangeListener(new KeyStatusChangeListener(), null);
    }

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
            if (DEBUG) Log.d(LOGTAG, "Key successfully added for session " + new String(sessionId));
        }
    }

    @Override
    protected void HandleKeyStatusChangeByDummyKey(String sessionId)
    {
        // MediaDrm.KeyStatus information listener is supported on M+, there is no need to use
        // dummy key id to report key status anymore.
    }
}
