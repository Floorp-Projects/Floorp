/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * This class exposes the Base64 URL encode/decode functions from Gecko. They are different from
 * android.util.Base64 in that they always use URL encoding, no padding, and are constant time. The
 * last bit is important when dealing with values that might be secret as we do with Web Push.
 */
/* package */ class Base64Utils {
  @WrapForJNI
  public static native byte[] decode(final String data);

  @WrapForJNI
  public static native String encode(final byte[] data);
}
