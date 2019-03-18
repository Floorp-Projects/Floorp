package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.LinkedList;

/**
 * This class provides an {@link InputStream} wrapper for a Gecko nsIChannel
 * (or really, nsIRequest).
 */
@WrapForJNI
@AnyThread
/* package */ class GeckoInputStream extends InputStream {
    private static final String LOGTAG = "GeckoInputStream";

    private LinkedList<ByteBuffer> mBuffers = new LinkedList<>();
    private boolean mEOF;
    private boolean mResumed;
    private Support mSupport;

    /**
     * This is only called via JNI. The support instance provides
     * callbacks for the native counterpart.
     *
     * @param support An instance of {@link Support}, used for native callbacks.
     */
    private GeckoInputStream(final @NonNull Support support) {
        mSupport = support;
    }

    @Override
    public synchronized void close() throws IOException {
        super.close();
        sendEof();
    }

    @Override
    public synchronized int available() throws IOException {
        final ByteBuffer buf = mBuffers.peekFirst();
        return buf != null ? buf.remaining() : 0;
    }

    @Override
    public synchronized int read() throws IOException {
        int expect = Integer.SIZE / 8;
        byte[] bytes = new byte[expect];

        int count = 0;
        while (count < expect) {
            long bytesRead = read(bytes, count, expect - count);
            if (bytesRead < 0) {
                return -1;
            }

            count += bytesRead;
        }

        final ByteBuffer buffer = ByteBuffer.wrap(bytes);
        return buffer.getInt();
    }

    @Override
    public int read(final @NonNull byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    @Override
    public synchronized int read(final @NonNull byte[] dest, final int offset, final int length)
            throws IOException {
        while (!mEOF && mBuffers.size() == 0) {
            // The underlying channel is suspended, so resume that before
            // waiting for a buffer.
            if (!mResumed) {
                mSupport.resume();
                mResumed = true;
            }

            try {
                wait();
            } catch (InterruptedException e) {
            }
        }

        if (mEOF && mBuffers.size() == 0) {
            // We have no data and we're not expecting more.
            return -1;
        }

        final ByteBuffer buf = mBuffers.peekFirst();
        final int readCount = Math.min(length, buf.remaining());
        buf.get(dest, offset, readCount);

        if (buf.remaining() == 0) {
            // We're done with this buffer, advance the queue.
            mBuffers.removeFirst();
        }

        return readCount;
    }

    /**
     * Called by native code to indicate that no more data will be
     * sent via {@link #appendBuffer}.
     */
    @WrapForJNI(calledFrom = "gecko")
    public synchronized void sendEof() {
        mEOF = true;
        notifyAll();
    }

    /**
     * Called by native code to provide data for this stream.
     *
     * @param buf the bytes
     * @throws IOException
     */
    @WrapForJNI(exceptionMode = "nsresult", calledFrom = "gecko")
    private synchronized void appendBuffer(final byte[] buf) throws IOException {
        ThreadUtils.assertOnGeckoThread();

        if (mEOF) {
            throw new IllegalStateException();
        }

        mBuffers.add(ByteBuffer.wrap(buf));
        notifyAll();
    }

    @WrapForJNI
    private static class Support extends JNIObject {
        @WrapForJNI(dispatchTo = "gecko")
        private native void resume();

        @Override // JNIObject
        protected void disposeNative() {
            throw new UnsupportedOperationException();
        }
    }
}
