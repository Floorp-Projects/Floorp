/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.util.SparseArray;

import org.mozilla.gecko.mozglue.SharedMemory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

final class SamplePool {
    private static final class Impl {
        private final String mName;
        private int mDefaultBufferSize = 4096;
        private final List<Sample> mRecycledSamples = new ArrayList<>();
        private final boolean mBufferless;

        private int mNextBufferId = Sample.NO_BUFFER + 1;
        private SparseArray<SampleBuffer> mBuffers = new SparseArray<>();

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
                return Sample.obtain();
            } else {
                return allocateSampleAndBuffer(size);
            }
        }

        private Sample allocateSampleAndBuffer(final int size) {
            final int id = mNextBufferId++;
            try {
                final SharedMemory shm = new SharedMemory(id, Math.max(size, mDefaultBufferSize));
                mBuffers.put((Integer)id, new SampleBuffer(shm));
                final Sample s = Sample.obtain();
                s.bufferId = id;
                return s;
            } catch (final NoSuchMethodException | IOException e) {
                mBuffers.delete(id);
                throw new UnsupportedOperationException(e);
            }
        }

        private synchronized SampleBuffer getBuffer(final int id) {
            return mBuffers.get(id);
        }

        private synchronized void recycle(final Sample recycled) {
            if (mBufferless || isUsefulSample(recycled)) {
                mRecycledSamples.add(recycled);
            } else {
                disposeSample(recycled);
            }
        }

        private boolean isUsefulSample(final Sample sample) {
            return mBuffers.get(sample.bufferId).capacity() >= mDefaultBufferSize;
        }

        private synchronized void clear() {
            for (final Sample s : mRecycledSamples) {
                disposeSample(s);
            }
            mRecycledSamples.clear();

            for (int i = 0; i < mBuffers.size(); ++i) {
                mBuffers.valueAt(i).dispose();
            }
            mBuffers.clear();
        }

        private void disposeSample(final Sample sample) {
            if (sample.bufferId != Sample.NO_BUFFER) {
                mBuffers.get(sample.bufferId).dispose();
                mBuffers.delete(sample.bufferId);
            }
            sample.dispose();
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
        final Sample input = mInputs.obtain(size);
        input.info.set(0, 0, 0, 0);
        return input;
    }

    /* package */ Sample obtainOutput(final MediaCodec.BufferInfo info) {
        final Sample output = mOutputs.obtain(info.size);
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

    /* package */ SampleBuffer getInputBuffer(final int id) {
        return mInputs.getBuffer(id);
    }

    /* package */ SampleBuffer getOutputBuffer(final int id) {
        return mOutputs.getBuffer(id);
    }
}
