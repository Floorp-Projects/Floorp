/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;

public final class CodecManager extends Service {
    private Binder mBinder = new ICodecManager.Stub() {
        @Override
        public ICodec createCodec() throws RemoteException {
            return new Codec();
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}