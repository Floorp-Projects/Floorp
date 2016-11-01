/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

// Non-default types used in interface.
import org.mozilla.gecko.media.IMediaDrmBridgeCallbacks;

interface IMediaDrmBridge {
    void setCallbacks(in IMediaDrmBridgeCallbacks callbacks);

    oneway void createSession(int createSessionToken,
                              int promiseId,
                              String initDataType,
                              in byte[] initData);

    oneway void updateSession(int promiseId,
                              String sessionId,
                              in byte[] response);

    oneway void closeSession(int promiseId, String sessionId);

    oneway void release();
}
