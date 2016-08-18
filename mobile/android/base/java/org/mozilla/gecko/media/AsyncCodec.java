/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec.BufferInfo;
import android.media.MediaFormat;
import android.os.Handler;
import android.view.Surface;

import java.nio.ByteBuffer;

// A wrapper interface that mimics the new {@link android.media.MediaCodec}
// asynchronous mode API in Lollipop.
public interface AsyncCodec {
    public interface Callbacks {
        void onInputBufferAvailable(AsyncCodec codec, int index);
        void onOutputBufferAvailable(AsyncCodec codec, int index, BufferInfo info);
        void onError(AsyncCodec codec, int error);
        void onOutputFormatChanged(AsyncCodec codec, MediaFormat format);
    }

    public abstract void setCallbacks(Callbacks callbacks, Handler handler);
    public abstract void configure(MediaFormat format, Surface surface, int flags);
    public abstract void start();
    public abstract void stop();
    public abstract void flush();
    public abstract void release();
    public abstract ByteBuffer getInputBuffer(int index);
    public abstract ByteBuffer getOutputBuffer(int index);
    public abstract void queueInputBuffer(int index, int offset, int size, long presentationTimeUs, int flags);
    public abstract void releaseOutputBuffer(int index, boolean render);
}
