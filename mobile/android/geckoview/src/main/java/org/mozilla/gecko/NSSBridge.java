/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.GeckoLoader;

import android.content.Context;
import org.mozilla.gecko.annotation.RobocopTarget;

public class NSSBridge {
    private static final String LOGTAG = "NSSBridge";

    private static native String nativeEncrypt(String aDb, String aValue);
    private static native String nativeDecrypt(String aDb, String aValue);

    @RobocopTarget
    static public String encrypt(final Context context, final String aValue) throws Exception {
        GeckoLoader.loadNSSLibs(context);

        String path = GeckoProfile.get(context).getDir().toString();
        return nativeEncrypt(path, aValue);
    }

    @RobocopTarget
    static public String encrypt(final Context context, final String profilePath,
                                 final String aValue) throws Exception {
        GeckoLoader.loadNSSLibs(context);

        return nativeEncrypt(profilePath, aValue);
    }

    @RobocopTarget
    static public String decrypt(final Context context, final String aValue) throws Exception {
        GeckoLoader.loadNSSLibs(context);

        String path = GeckoProfile.get(context).getDir().toString();
        return nativeDecrypt(path, aValue);
    }

    @RobocopTarget
    static public String decrypt(final Context context, final String profilePath,
                                 final String aValue) throws Exception {
        GeckoLoader.loadNSSLibs(context);

        return nativeDecrypt(profilePath, aValue);
    }
}
