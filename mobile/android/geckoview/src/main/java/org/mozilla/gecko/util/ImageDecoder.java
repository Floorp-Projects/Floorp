/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.Bitmap;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.geckoview.GeckoResult;

/** Provides access to Gecko's Image processing library. */
@AnyThread
public class ImageDecoder {
  private static ImageDecoder instance;

  private ImageDecoder() {}

  public static ImageDecoder instance() {
    if (instance == null) {
      instance = new ImageDecoder();
    }

    return instance;
  }

  @WrapForJNI(dispatchTo = "gecko", stubName = "Decode")
  private static native void nativeDecode(
      final String uri, final int desiredLength, GeckoResult<Bitmap> result);

  /**
   * Fetches and decodes an image at the specified location. This method supports SVG, PNG, Bitmap
   * and other formats supported by Gecko.
   *
   * @param uri location of the image. Can be either a remote https:// location, file:/// if the
   *     file is local or a resource://android/ if the file is located inside the APK.
   *     <p>e.g. if the image file is locate at /assets/test.png inside the apk, set the uri to
   *     resource://android/assets/test.png.
   * @return A {@link GeckoResult} to the decoded image.
   */
  @NonNull
  public GeckoResult<Bitmap> decode(final @NonNull String uri) {
    return decode(uri, 0);
  }

  /**
   * Fetches and decodes an image at the specified location and resizes it to the desired length.
   * This method supports SVG, PNG, Bitmap and other formats supported by Gecko.
   *
   * <p>Note: The final size might differ slightly from the requested output.
   *
   * @param uri location of the image. Can be either a remote https:// location, file:/// if the
   *     file is local or a resource://android/ if the file is located inside the APK.
   *     <p>e.g. if the image file is locate at /assets/test.png inside the apk, set the uri to
   *     resource://android/assets/test.png.
   * @param desiredLength Longest size for the image in device pixel units. The resulting image
   *     might be slightly different if the image cannot be resized efficiently. If desiredLength is
   *     0 then the image will be decoded to its natural size.
   * @return A {@link GeckoResult} to the decoded image.
   */
  @NonNull
  public GeckoResult<Bitmap> decode(final @NonNull String uri, final int desiredLength) {
    if (uri == null) {
      throw new IllegalArgumentException("Uri cannot be null");
    }

    final GeckoResult<Bitmap> result = new GeckoResult<>();

    if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
      nativeDecode(uri, desiredLength, result);
    } else {
      GeckoThread.queueNativeCallUntil(
          GeckoThread.State.PROFILE_READY,
          this,
          "nativeDecode",
          String.class,
          uri,
          int.class,
          desiredLength,
          GeckoResult.class,
          result);
    }

    return result;
  }
}
