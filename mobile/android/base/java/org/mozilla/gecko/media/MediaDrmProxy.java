/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.gecko.media;

import java.util.UUID;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.util.Log;
import android.os.Build;

public final class MediaDrmProxy {
    private static final String LOGTAG = "GeckoMediaDrmProxy";
    private static final boolean DEBUG = false;
    private static final UUID WIDEVINE_SCHEME_UUID =
            new UUID(0xedef8ba979d64aceL, 0xa3c827dcd51d21edL);

    private static final String WIDEVINE_KEY_SYSTEM = "com.widevine.alpha";
    @WrapForJNI
    private static final String AAC = "audio/mp4a-latm";
    @WrapForJNI
    private static final String AVC = "video/avc";
    @WrapForJNI
    private static final String VORBIS = "audio/vorbis";
    @WrapForJNI
    private static final String VP8 = "video/x-vnd.on2.vp8";
    @WrapForJNI
    private static final String VP9 = "video/x-vnd.on2.vp9";
    @WrapForJNI
    private static final String OPUS = "audio/opus";

    private static boolean isSystemSupported() {
        // Support versions >= LOLLIPOP
        if (AppConstants.Versions.preLollipop) {
            if (DEBUG) Log.d(LOGTAG, "System Not supported !!, current SDK version is " + Build.VERSION.SDK_INT);
            return false;
        }
        return true;
    }

    @WrapForJNI
    public static boolean isSchemeSupported(String keySystem) {
        if (!isSystemSupported()) {
            return false;
        }
        if (keySystem.equals(WIDEVINE_KEY_SYSTEM)) {
            return MediaDrm.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID)
                    && MediaCrypto.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID);
        }
        if (DEBUG) Log.d(LOGTAG, "isSchemeSupported key sytem = " + keySystem);
        return false;
    }

    @WrapForJNI
    public static boolean IsCryptoSchemeSupported(String keySystem,
                                                  String container) {
        if (!isSystemSupported()) {
            return false;
        }
        if (keySystem.equals(WIDEVINE_KEY_SYSTEM)) {
            return MediaDrm.isCryptoSchemeSupported(WIDEVINE_SCHEME_UUID, container);
        }
        if (DEBUG) Log.d(LOGTAG, "cannot decrypt key sytem = " + keySystem + ", container = " + container);
        return false;
    }

    @WrapForJNI
    public static boolean CanDecode(String mimeType) {
        for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
            MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            if (info.isEncoder()) {
                continue;
            }
            for (String m : info.getSupportedTypes()) {
                if (m.equals(mimeType)) {
                  return true;
                }
            }
        }
        if (DEBUG) Log.d(LOGTAG, "cannot decode mimetype = " + mimeType);
        return false;
    }
}
