package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * This class exposes the Base64 URL encode/decode functions from Gecko. They are different
 * from android.util.Base64 in that they always use URL encoding, no padding, and are
 * constant time. The last bit is important when dealing with values that might be secret
 * as we do with Web Push.
 */
/* package */ class Base64Utils {
    @WrapForJNI public static native byte[] decode(final String data);
    @WrapForJNI public static native String encode(final byte[] data);
}
