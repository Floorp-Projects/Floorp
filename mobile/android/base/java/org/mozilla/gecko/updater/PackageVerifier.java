/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.util.Log;

import org.mozilla.apache.commons.codec.binary.Hex;
import org.mozilla.gecko.BuildConfig;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;

public class PackageVerifier {
    private static final String LOGTAG = "GeckoPackageVerifier";
    private static final boolean DEBUG = false;

    static boolean verifyDownloadedPackage(final UpdatesPrefs prefs, final File updateFile, final int bufSize) {
        MessageDigest digest = createMessageDigest(prefs.getLastHashFunction());
        if (digest == null)
            return false;

        InputStream input = null;

        try {
            input = new BufferedInputStream(new FileInputStream(updateFile));

            byte[] buf = new byte[bufSize];
            int len;
            while ((len = input.read(buf, 0, bufSize)) > 0) {
                digest.update(buf, 0, len);
            }
        } catch (java.io.IOException e) {
            if (DEBUG) {
                Log.e(LOGTAG, "Failed to verify update package: ", e);
            }
            return false;
        } finally {
            try {
                if (input != null)
                    input.close();
            } catch (java.io.IOException e) { }
        }

        String hex = Hex.encodeHexString(digest.digest());
        if (!hex.equals(prefs.getLastHashValue())) {
            if (DEBUG) {
                Log.e(LOGTAG, "Package hash does not match");
            }
            return false;
        }

        return true;
    }

    private static MessageDigest createMessageDigest(String hashFunction) {
        String javaHashFunction;

        if ("sha512".equalsIgnoreCase(hashFunction)) {
            javaHashFunction = "SHA-512";
        } else {
            if (DEBUG) {
                Log.e(LOGTAG, "Unhandled hash function: " + hashFunction);
            }
            return null;
        }

        try {
            return MessageDigest.getInstance(javaHashFunction);
        } catch (java.security.NoSuchAlgorithmException e) {
            if (DEBUG) {
                Log.e(LOGTAG, "Couldn't find algorithm " + javaHashFunction, e);
            }
            return null;
        }
    }
}
