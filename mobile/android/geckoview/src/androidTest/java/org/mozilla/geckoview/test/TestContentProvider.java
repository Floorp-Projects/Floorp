/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.ParcelFileDescriptor.AutoCloseOutputStream;
import android.util.Log;
import java.io.FileNotFoundException;
import java.io.IOException;

/** TestContentProvider provides any data via content resolver by content:// */
public class TestContentProvider extends ContentProvider {
  private static final String LOGTAG = "TestContentProvider";
  private static byte[] sTestData;
  private static String sMimeType;

  @Override
  public boolean onCreate() {
    return true;
  }

  @Override
  public String getType(final Uri uri) {
    return sMimeType;
  }

  @Override
  public Cursor query(
      final Uri uri,
      final String[] projection,
      final String selection,
      final String[] selectionArgs,
      final String sortOrder) {
    return null;
  }

  @Override
  public Uri insert(final Uri uri, final ContentValues values) {
    return null;
  }

  @Override
  public int delete(final Uri uri, final String selection, final String[] selectionArgs) {
    return 0;
  }

  @Override
  public int update(
      final Uri uri,
      final ContentValues values,
      final String selection,
      final String[] selectionArgs) {
    return 0;
  }

  @Override
  public ParcelFileDescriptor openFile(final Uri uri, final String mode)
      throws FileNotFoundException {
    if (sTestData == null) {
      throw new FileNotFoundException("No test data for: " + uri);
    }

    ParcelFileDescriptor[] pipe = null;
    AutoCloseOutputStream outputStream = null;

    try {
      try {
        pipe = ParcelFileDescriptor.createPipe();
        outputStream = new AutoCloseOutputStream(pipe[1]);
        outputStream.write(sTestData);
        outputStream.flush();
        return pipe[0];
      } finally {
        if (outputStream != null) {
          outputStream.close();
        }
        if (pipe != null && pipe[1] != null) {
          pipe[1].close();
        }
      }
    } catch (IOException e) {
      Log.e(LOGTAG, "openFile throws an I/O exception: ", e);
    }

    throw new FileNotFoundException("Could not open uri for: " + uri);
  }

  /**
   * Set test data that is used from content resolver.
   *
   * @param data test data
   * @param mimeType A mime type of test data.
   */
  public static void setTestData(final byte[] data, final String mimeType) {
    sTestData = data;
    sMimeType = mimeType;
  }
}
