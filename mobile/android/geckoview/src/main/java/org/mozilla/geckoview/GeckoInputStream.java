/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.LinkedList;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * This class provides an {@link InputStream} wrapper for a Gecko nsIChannel (or really,
 * nsIRequest).
 */
@WrapForJNI
@AnyThread
/* package */ class GeckoInputStream extends InputStream {
  private static final String LOGTAG = "GeckoInputStream";

  private LinkedList<ByteBuffer> mBuffers = new LinkedList<>();
  private boolean mEOF;
  private boolean mClosed;
  private boolean mHaveError;
  private long mReadTimeout;
  private boolean mResumed;
  private Support mSupport;

  /**
   * This is only called via JNI. The support instance provides callbacks for the native
   * counterpart.
   *
   * @param support An instance of {@link Support}, used for native callbacks.
   */
  private GeckoInputStream(final @NonNull Support support) {
    mSupport = support;
  }

  public void setReadTimeoutMillis(final long millis) {
    mReadTimeout = millis;
  }

  @Override
  public synchronized void close() throws IOException {
    super.close();
    mClosed = true;

    if (mSupport != null) {
      mSupport.close();
      mSupport = null;
    }
  }

  @Override
  public synchronized int available() throws IOException {
    if (mClosed) {
      return 0;
    }

    final ByteBuffer buf = mBuffers.peekFirst();
    return buf != null ? buf.remaining() : 0;
  }

  private void ensureNotClosed() throws IOException {
    if (mClosed) {
      throw new IOException("Stream is closed");
    }
  }

  @Override
  public synchronized int read() throws IOException {
    ensureNotClosed();

    final int expect = Integer.SIZE / 8;
    final byte[] bytes = new byte[expect];

    int count = 0;
    while (count < expect) {
      final long bytesRead = read(bytes, count, expect - count);
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
    ensureNotClosed();

    final long startTime = System.currentTimeMillis();
    while (!mEOF && mBuffers.size() == 0) {
      if (mReadTimeout > 0 && (System.currentTimeMillis() - startTime) >= mReadTimeout) {
        throw new IOException("Timed out");
      }

      // The underlying channel is suspended, so resume that before
      // waiting for a buffer.
      if (!mResumed) {
        mSupport.resume();
        mResumed = true;
      }

      try {
        wait(mReadTimeout);
      } catch (final InterruptedException e) {
      }
    }

    if (mEOF && mBuffers.size() == 0) {
      if (mHaveError) {
        throw new IOException("Unknown error");
      }

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

  /** Called by native code to indicate that no more data will be sent via {@link #appendBuffer}. */
  @WrapForJNI(calledFrom = "gecko")
  public synchronized void sendEof() {
    if (mEOF) {
      throw new IllegalStateException("Already have EOF");
    }

    mEOF = true;
    notifyAll();
  }

  /** Called by native code to indicate that there was an error while reading the stream. */
  @WrapForJNI(calledFrom = "gecko")
  public synchronized void sendError() {
    if (mEOF) {
      throw new IllegalStateException("Already have EOF");
    }

    mEOF = true;
    mHaveError = true;
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

    if (mClosed) {
      throw new IllegalStateException("Stream is closed");
    }

    if (mEOF) {
      throw new IllegalStateException("EOF, no more data expected");
    }

    mBuffers.add(ByteBuffer.wrap(buf));
    notifyAll();
  }

  @WrapForJNI
  private static class Support extends JNIObject {
    @WrapForJNI(dispatchTo = "gecko")
    private native void resume();

    @WrapForJNI(dispatchTo = "gecko")
    private native void close();

    @Override // JNIObject
    protected void disposeNative() {
      throw new UnsupportedOperationException();
    }
  }
}
