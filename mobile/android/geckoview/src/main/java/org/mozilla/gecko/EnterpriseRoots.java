/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.IOException;

import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;

import java.security.cert.Certificate;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;

import java.util.ArrayList;
import java.util.Enumeration;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.util.Log;

// This class implements the functionality needed to find third-party root
// certificates that have been added to the android CA store.
public class EnterpriseRoots {
    private static final String LOGTAG = "EnterpriseRoots";

    // Gecko calls this function from C++ to find third-party root certificates
    // it can use as trust anchors for TLS connections.
    @WrapForJNI
    private static byte[][] gatherEnterpriseRoots() {

        // The KeyStore "AndroidCAStore" contains the certificates we're
        // interested in.
        KeyStore ks;
        try {
            ks = KeyStore.getInstance("AndroidCAStore");
        } catch (KeyStoreException kse) {
            Log.e(LOGTAG, "getInstance() failed", kse);
            return new byte[0][0];
        }
        try {
            ks.load(null);
        } catch (CertificateException ce) {
            Log.e(LOGTAG, "load() failed", ce);
            return new byte[0][0];
        } catch (IOException ioe) {
            Log.e(LOGTAG, "load() failed", ioe);
            return new byte[0][0];
        } catch (NoSuchAlgorithmException nsae) {
            Log.e(LOGTAG, "load() failed", nsae);
            return new byte[0][0];
        }
        // Given the KeyStore, we get an identifier for each object in it. For
        // each one that is a Certificate, we try to distinguish between
        // entries that shipped with the OS and entries that were added by the
        // user or an administrator. The former we ignore and the latter we
        // collect in an array of byte arrays and return.
        Enumeration<String> aliases;
        try {
            aliases = ks.aliases();
        } catch (KeyStoreException kse) {
            Log.e(LOGTAG, "aliases() failed", kse);
            return new byte[0][0];
        }
        ArrayList<byte[]> roots = new ArrayList<byte[]>();
        while (aliases.hasMoreElements()) {
            String alias = aliases.nextElement();
            boolean isCertificate;
            try {
                isCertificate = ks.isCertificateEntry(alias);
            } catch (KeyStoreException kse) {
                Log.e(LOGTAG, "isCertificateEntry() failed", kse);
                continue;
            }
            // Built-in certificate aliases start with "system:", whereas
            // 3rd-party certificate aliases start with "user:". It's
            // unfortunate to be relying on this implementation detail, but
            // there appears to be no other way to differentiate between the
            // two.
            if (isCertificate && alias.startsWith("user:")) {
                Certificate certificate;
                try {
                    certificate = ks.getCertificate(alias);
                } catch (KeyStoreException kse) {
                    Log.e(LOGTAG, "getCertificate() failed", kse);
                    continue;
                }
                try {
                    roots.add(certificate.getEncoded());
                } catch (CertificateEncodingException cee) {
                    Log.e(LOGTAG, "getEncoded() failed", cee);
                }
            }
        }
        Log.d(LOGTAG, "found " + roots.size() + " enterprise roots");
        return roots.toArray(new byte[0][0]);
    }
}
