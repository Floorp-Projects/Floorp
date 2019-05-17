/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.ArrayList;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.GeckoBundle;

import android.util.Log;

public class WebAuthnTokenManager {
    private static final String LOGTAG = "WebAuthnTokenManager";

    public static class WebAuthnPublicCredential {
        public final byte[] mId;
        public final byte mTransports;

        public WebAuthnPublicCredential(final byte[] aId, final byte aTransports) {
            this.mId = aId;
            this.mTransports = aTransports;
        }

        static ArrayList<WebAuthnPublicCredential> CombineBuffers(
                final Object[] idObjectList, final ByteBuffer transportList) {
            if (idObjectList.length != transportList.remaining()) {
                throw new RuntimeException("Couldn't extract allowed list!");
            }

            ArrayList<WebAuthnPublicCredential> credList =
                new ArrayList<WebAuthnPublicCredential>();

            byte[] transportBytes = new byte[transportList.remaining()];
            transportList.get(transportBytes);

            for (int i = 0; i < idObjectList.length; i++) {
                final ByteBuffer id = (ByteBuffer)idObjectList[i];
                byte[] idBytes = new byte[id.remaining()];
                id.get(idBytes);

                credList.add(new WebAuthnPublicCredential(idBytes, transportBytes[i]));
            }
            return credList;
        }
    }

    // From WebAuthentication.webidl
    public enum AttestationPreference {
      NONE,
      INDIRECT,
      DIRECT,
    }

    public interface WebAuthnMakeCredentialResponse {
        void onSuccess(final byte[] clientDataJson, final byte[] keyHandle,
                       final byte[] attestationObject);
        void onFailure(String errorCode);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void webAuthnMakeCredential(final GeckoBundle credentialBundle,
                                               final ByteBuffer userId,
                                               final ByteBuffer challenge,
                                               final Object[] idList,
                                               final ByteBuffer transportList,
                                               final GeckoBundle authenticatorSelection,
                                               final GeckoBundle extensions) {
        ArrayList<WebAuthnPublicCredential> excludeList;

        // TODO: Return a GeckoResult instead, Bug 1550116

        if (!GeckoAppShell.isFennec()) {
            Log.w(LOGTAG, "Currently only supported on Fennec");
            webAuthnMakeCredentialReturnError("NOT_SUPPORTED_ERR");
            return;
        }

        byte[] challBytes = new byte[challenge.remaining()];
        byte[] userBytes = new byte[userId.remaining()];
        try {
            challenge.get(challBytes);
            userId.get(userBytes);

            excludeList = WebAuthnPublicCredential.CombineBuffers(idList,
                                                                  transportList);
        } catch (RuntimeException e) {
            Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
            webAuthnMakeCredentialReturnError("UNKNOWN_ERR");
            return;
        }

        WebAuthnMakeCredentialResponse handler = new WebAuthnMakeCredentialResponse() {
            @Override
            public void onSuccess(final byte[] clientDataJson, final byte[] keyHandle,
                                  final byte[] attestationObject) {
                webAuthnMakeCredentialFinish(clientDataJson, keyHandle,
                                             attestationObject);
            }
            @Override
            public void onFailure(final String errorCode) {
                webAuthnMakeCredentialReturnError(errorCode);
            }
        };

        try {
            final Class<?> cls = Class.forName("org.mozilla.gecko.util.WebAuthnUtils");
            Class<?>[] argTypes = new Class<?>[] { GeckoBundle.class, byte[].class, byte[].class,
                                                   WebAuthnPublicCredential[].class,
                                                   GeckoBundle.class, GeckoBundle.class,
                                                   WebAuthnMakeCredentialResponse.class };
            Method make = cls.getDeclaredMethod("makeCredential", argTypes);

            make.invoke(null, credentialBundle, userBytes, challBytes,
                        excludeList.toArray(new WebAuthnPublicCredential[0]),
                        authenticatorSelection, extensions, handler);
        } catch (Exception e) {
            Log.w(LOGTAG, "Couldn't run WebAuthnUtils", e);
            webAuthnMakeCredentialReturnError("UNKNOWN_ERR");
            return;
        }
    }

    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnMakeCredentialFinish(final byte[] clientDataJson,
                                                                  final byte[] keyHandle,
                                                                  final byte[] attestationObject);
    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnMakeCredentialReturnError(String errorCode);

    public interface WebAuthnGetAssertionResponse {
        void onSuccess(final byte[] clientDataJson, final byte[] keyHandle,
                       final byte[] authData, final byte[] signature,
                       final byte[] userHandle);
        void onFailure(String errorCode);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void webAuthnGetAssertion(final ByteBuffer challenge,
                                             final Object[] idList,
                                             final ByteBuffer transportList,
                                             final GeckoBundle assertionBundle,
                                             final GeckoBundle extensions) {
        ArrayList<WebAuthnPublicCredential> allowList;

        // TODO: Return a GeckoResult instead, Bug 1550116

        if (!GeckoAppShell.isFennec()) {
            Log.w(LOGTAG, "Currently only supported on Fennec");
            webAuthnGetAssertionReturnError("NOT_SUPPORTED_ERR");
            return;
        }

        byte[] challBytes = new byte[challenge.remaining()];
        try {
            challenge.get(challBytes);
            allowList = WebAuthnPublicCredential.CombineBuffers(idList,
                                                                transportList);
        } catch (RuntimeException e) {
            Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
            webAuthnGetAssertionReturnError("UNKNOWN_ERR");
            return;
        }

        WebAuthnGetAssertionResponse handler = new WebAuthnGetAssertionResponse() {
            @Override
            public void onSuccess(final byte[] clientDataJson, final byte[] keyHandle,
                                  final byte[] authData, final byte[] signature,
                                  final byte[] userHandle) {
                webAuthnGetAssertionFinish(clientDataJson, keyHandle, authData,
                                           signature, userHandle);
            }
            @Override
            public void onFailure(final String errorCode) {
                webAuthnGetAssertionReturnError(errorCode);
            }
        };

        try {
            final Class<?> cls = Class.forName("org.mozilla.gecko.util.WebAuthnUtils");
            Class<?>[] argTypes = new Class<?>[] { byte[].class, WebAuthnPublicCredential[].class,
                                                   GeckoBundle.class, GeckoBundle.class,
                                                   WebAuthnGetAssertionResponse.class };
            Method make = cls.getDeclaredMethod("getAssertion", argTypes);

            make.invoke(null, challBytes,
                        allowList.toArray(new WebAuthnPublicCredential[0]),
                        assertionBundle, extensions, handler);
        } catch (Exception e) {
            Log.w(LOGTAG, "Couldn't run WebAuthnUtils", e);
            webAuthnGetAssertionReturnError("UNKNOWN_ERR");
            return;
        }
    }

    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnGetAssertionFinish(final byte[] clientDataJson,
                                                                final byte[] keyHandle,
                                                                final byte[] authData,
                                                                final byte[] signature,
                                                                final byte[] userHandle);
    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnGetAssertionReturnError(String errorCode);
}
