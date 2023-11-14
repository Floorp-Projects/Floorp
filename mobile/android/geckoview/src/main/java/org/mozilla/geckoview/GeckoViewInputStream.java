/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import org.mozilla.gecko.annotation.WrapForJNI;

/** This class provides a Gecko nsIInputStream wrapper for a Java {@link InputStream}. */
@WrapForJNI
@AnyThread
/* package */ class GeckoViewInputStream {
  private static final String LOGTAG = "GeckoViewInputStream";
  private static final int BUFFER_SIZE = 4096;

  protected final ByteBuffer mBuffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
  private ReadableByteChannel mChannel;
  private InputStream mIs = null;
  private boolean mMustGetData = true;
  private int mPos = 0;
  private int mSize;

  /**
   * Set an input stream.
   *
   * @param is the {@link InputStream} to set.
   */
  protected void setInputStream(final @NonNull InputStream is) {
    mIs = is;
    mChannel = Channels.newChannel(is);
  }

  /**
   * Check if there is a stream.
   *
   * @return true if there is no stream.
   */
  public boolean isClosed() {
    return mIs == null;
  }

  /**
   * Called by native code to get the number of available bytes in the underlying stream.
   *
   * @return the number of available bytes.
   */
  public int available() {
    if (mIs == null || mSize == -1) {
      return 0;
    }
    try {
      return Math.max(mIs.available(), mMustGetData ? 0 : mSize - mPos);
    } catch (final IOException e) {
      Log.e(LOGTAG, "Cannot get the number of available bytes", e);
      return 0;
    }
  }

  /** Close the underlying stream. */
  public void close() {
    if (mIs == null) {
      return;
    }
    try {
      mChannel.close();
    } catch (final IOException e) {
      Log.e(LOGTAG, "Cannot close the channel", e);
    } finally {
      mChannel = null;
    }

    try {
      mIs.close();
    } catch (final IOException e) {
      Log.e(LOGTAG, "Cannot close the stream", e);
    } finally {
      mIs = null;
    }
  }

  /**
   * Called by native code to notify that the data have been consumed.
   *
   * @param length the number of consumed bytes.
   * @return the position in the buffer.
   */
  public long consumedData(final int length) {
    mPos += length;
    if (mPos >= mSize) {
      mPos = 0;
      mMustGetData = true;
    }
    return mPos;
  }

  /**
   * Check that the underlying stream starts with one of the given headers.
   *
   * @param headers the headers to check.
   * @return true if one of the headers is found.
   */
  protected boolean checkHeaders(final @NonNull byte[][] headers) throws IOException {
    read(0);
    for (final byte[] header : headers) {
      final int headerLength = header.length;
      if (mSize < headerLength) {
        continue;
      }
      int i = 0;
      for (; i < headerLength; i++) {
        if (mBuffer.get(i) != header[i]) {
          break;
        }
      }
      if (i == headerLength) {
        return true;
      }
    }
    return false;
  }

  /**
   * Called by native code to read some bytes in the underlying stream.
   *
   * @param aCount the number of bytes to read.
   * @return the number of read bytes, -1 in case of EOF.
   * @throws IOException if an error occurs.
   */
  @WrapForJNI(exceptionMode = "nsresult")
  public int read(final long aCount) throws IOException {
    if (mIs == null) {
      Log.e(LOGTAG, "The stream is closed.");
      throw new IllegalStateException("Stream is closed");
    }

    if (!mMustGetData) {
      return (int) Math.min((long) (mSize - mPos), aCount);
    }

    mMustGetData = false;
    mBuffer.clear();

    try {
      mSize = mChannel.read(mBuffer);
    } catch (final IOException e) {
      Log.e(LOGTAG, "Cannot read some bytes", e);
      throw e;
    }
    if (mSize == -1) {
      return -1;
    }

    return (int) Math.min((long) mSize, aCount);
  }
}
