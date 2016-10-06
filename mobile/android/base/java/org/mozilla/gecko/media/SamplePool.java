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
    private final class Impl {
        private final String mName;
        private int mNextId = 0;
        private int mDefaultBufferSize = 4096;
        private final List<Sample> mRecycledSamples = new ArrayList<>();

        private Impl(String name) {
            mName = name;
        }

        private void setDefaultBufferSize(int size) {
            mDefaultBufferSize = size;
        }

        private synchronized Sample allocate(int size)  {
            Sample sample;
            if (!mRecycledSamples.isEmpty()) {
                sample = mRecycledSamples.remove(0);
                sample.info.set(0, 0, 0, 0);
            } else {
                SharedMemory shm = null;
                try {
                    shm = new SharedMemory(mNextId++, Math.max(size, mDefaultBufferSize));
                } catch (NoSuchMethodException e) {
                    e.printStackTrace();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                if (shm != null) {
                    sample = Sample.create(shm);
                } else {
                    sample = Sample.create();
                }
            }

            return sample;
        }

        private synchronized void recycle(Sample recycled) {
            if (recycled.buffer.capacity() >= mDefaultBufferSize) {
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

    /* package */ SamplePool(String name) {
        mInputs = new Impl(name + " input buffer pool");
        mOutputs = new Impl(name + " output buffer pool");
    }

    /* package */ void setInputBufferSize(int size) {
        mInputs.setDefaultBufferSize(size);
    }

    /* package */ void setOutputBufferSize(int size) {
        mOutputs.setDefaultBufferSize(size);
    }

    /* package */ Sample obtainInput(int size) {
        return mInputs.allocate(size);
    }

    /* package */ Sample obtainOutput(MediaCodec.BufferInfo info) {
        Sample output = mOutputs.allocate(info.size);
        output.info.set(0, info.size, info.presentationTimeUs, info.flags);
        return output;
    }

    /* package */ void recycleInput(Sample sample) {
        sample.cryptoInfo = null;
        mInputs.recycle(sample);
    }

    /* package */ void recycleOutput(Sample sample) {
        mOutputs.recycle(sample);
    }

    /* package */ void reset() {
        mInputs.clear();
        mOutputs.clear();
    }
}
