/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;

import org.mozilla.gecko.mozglue.SharedMemory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

final class SamplePool {
    private static final class Impl {
        private final String mName;
        private int mNextId = 0;
        private int mDefaultBufferSize = 4096;
        private final List<Sample> mRecycledSamples = new ArrayList<>();
        private final boolean mBufferless;

        private Impl(final String name, final boolean bufferless) {
            mName = name;
            mBufferless = bufferless;
        }

        private void setDefaultBufferSize(final int size) {
            if (mBufferless) {
                throw new IllegalStateException("Setting buffer size of a bufferless pool is not allowed");
            }
            mDefaultBufferSize = size;
        }

        private synchronized Sample obtain(final int size) {
            if (!mRecycledSamples.isEmpty()) {
                return mRecycledSamples.remove(0);
            }

            if (mBufferless) {
                return Sample.create();
            } else {
                return allocateSharedMemorySample(size);
            }
        }

        private Sample allocateSharedMemorySample(final int size) {
            SharedMemory shm = null;
            try {
                shm = new SharedMemory(mNextId++, Math.max(size, mDefaultBufferSize));
            } catch (NoSuchMethodException | IOException e) {
                throw new UnsupportedOperationException(e);
            }

            return Sample.create(shm);
        }

        private synchronized void recycle(final Sample recycled) {
            if (mBufferless || recycled.buffer.capacity() >= mDefaultBufferSize) {
                mRecycledSamples.add(recycled);
            } else {
                recycled.dispose();
            }
        }

        private synchronized void clear() {
            for (Sample s : mRecycledSamples) {
                s.dispose();
            }

            mRecycledSamples.clear();
        }

        @Override
        protected void finalize() {
            clear();
        }
    }

    private final Impl mInputs;
    private final Impl mOutputs;

    /* package */ SamplePool(final String name, final boolean renderToSurface) {
        mInputs = new Impl(name + " input sample pool", false);
        // Buffers are useless when rendering to surface.
        mOutputs = new Impl(name + " output sample pool", renderToSurface);
    }

    /* package */ void setInputBufferSize(final int size) {
        mInputs.setDefaultBufferSize(size);
    }

    /* package */ void setOutputBufferSize(final int size) {
        mOutputs.setDefaultBufferSize(size);
    }

    /* package */ Sample obtainInput(final int size) {
        Sample input = mInputs.obtain(size);
        input.info.set(0, 0, 0, 0);
        return input;
    }

    /* package */ Sample obtainOutput(final MediaCodec.BufferInfo info) {
        Sample output = mOutputs.obtain(info.size);
        output.info.set(0, info.size, info.presentationTimeUs, info.flags);
        return output;
    }

    /* package */ void recycleInput(final Sample sample) {
        sample.cryptoInfo = null;
        mInputs.recycle(sample);
    }

    /* package */ void recycleOutput(final Sample sample) {
        mOutputs.recycle(sample);
    }

    /* package */ void reset() {
        mInputs.clear();
        mOutputs.clear();
    }
}
