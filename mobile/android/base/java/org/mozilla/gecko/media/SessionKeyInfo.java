/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.WrapForJNI;

@WrapForJNI
public final class SessionKeyInfo {
    public byte[] keyId;
    public int status;

    public SessionKeyInfo(byte[] keyId, int status) {
        this.keyId = keyId;
        this.status = status;
    }
}
