/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.GeckoBundle;

import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.util.Base64;
import android.util.Log;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.common.Transport;

import com.google.android.gms.fido.fido2.Fido2ApiClient;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.Algorithm;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticationExtensions;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.EC2Algorithm;
import com.google.android.gms.fido.fido2.api.common.FidoAppIdExtension;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRpEntity;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialType;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialUserEntity;
import com.google.android.gms.fido.fido2.api.common.RSAAlgorithm;
import com.google.android.gms.tasks.Task;

/* package */ class WebAuthnTokenManager {
    private static final String LOGTAG = "WebAuthnTokenManager";

    // from u2fhid-capi.h
    private static final byte AUTHENTICATOR_TRANSPORT_USB = 1;
    private static final byte AUTHENTICATOR_TRANSPORT_NFC = 2;
    private static final byte AUTHENTICATOR_TRANSPORT_BLE = 4;

    private static final Algorithm[] SUPPORTED_ALGORITHMS = {
        EC2Algorithm.ES256, EC2Algorithm.ES384, EC2Algorithm.ES512,
        EC2Algorithm.ED256, /* no ED384 */      EC2Algorithm.ED512,
        RSAAlgorithm.PS256, RSAAlgorithm.PS384, RSAAlgorithm.PS512,
        RSAAlgorithm.RS256, RSAAlgorithm.RS384, RSAAlgorithm.RS512
    };

    private static List<Transport> getTransportsForByte(final byte transports) {
        final ArrayList<Transport> result = new ArrayList<Transport>();
        if ((transports & AUTHENTICATOR_TRANSPORT_USB) == AUTHENTICATOR_TRANSPORT_USB) {
            result.add(Transport.USB);
        }
        if ((transports & AUTHENTICATOR_TRANSPORT_NFC) == AUTHENTICATOR_TRANSPORT_NFC) {
            result.add(Transport.NFC);
        }
        if ((transports & AUTHENTICATOR_TRANSPORT_BLE) == AUTHENTICATOR_TRANSPORT_BLE) {
            result.add(Transport.BLUETOOTH_LOW_ENERGY);
        }

        return result;
    }

    public static class WebAuthnPublicCredential {
        public final byte[] id;
        public final byte transports;

        public WebAuthnPublicCredential(final byte[] aId, final byte aTransports) {
            this.id = aId;
            this.transports = aTransports;
        }

        static ArrayList<WebAuthnPublicCredential> CombineBuffers(
                final Object[] idObjectList, final ByteBuffer transportList) {
            if (idObjectList.length != transportList.remaining()) {
                throw new RuntimeException("Couldn't extract allowed list!");
            }

            final ArrayList<WebAuthnPublicCredential> credList =
                new ArrayList<WebAuthnPublicCredential>();

            final byte[] transportBytes = new byte[transportList.remaining()];
            transportList.get(transportBytes);

            for (int i = 0; i < idObjectList.length; i++) {
                final ByteBuffer id = (ByteBuffer)idObjectList[i];
                final byte[] idBytes = new byte[id.remaining()];
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

    public static class MakeCredentialResponse {
        public final byte[] clientDataJson;
        public final byte[] keyHandle;
        public final byte[] attestationObject;

        public MakeCredentialResponse(final byte[] clientDataJson, final byte[] keyHandle, final byte[] attestationObject) {
            this.clientDataJson = clientDataJson;
            this.keyHandle = keyHandle;
            this.attestationObject = attestationObject;
        }
    }

    public static class Exception extends RuntimeException {
        public Exception(final String error) {
            super(error);
        }
    }

    public static GeckoResult<MakeCredentialResponse> makeCredential(final GeckoBundle credentialBundle,
                                                                     final byte[] userId, final byte[] challenge,
                                                                     final WebAuthnTokenManager.WebAuthnPublicCredential[] excludeList,
                                                                     final GeckoBundle authenticatorSelection,
                                                                     final GeckoBundle extensions) {
        if (!credentialBundle.containsKey("isWebAuthn")) {
            // FIDO U2F not supported by Android (for us anyway) at this time
            return GeckoResult.fromException(new WebAuthnTokenManager.Exception("NOT_SUPPORTED_ERR"));
        }

        final PublicKeyCredentialCreationOptions.Builder requestBuilder =
                new PublicKeyCredentialCreationOptions.Builder();

        final List<PublicKeyCredentialParameters> params =
                new ArrayList<PublicKeyCredentialParameters>();

        // WebAuthn supports more algorithms
        for (final Algorithm algo : SUPPORTED_ALGORITHMS) {
            params.add(new PublicKeyCredentialParameters(
                    PublicKeyCredentialType.PUBLIC_KEY.toString(),
                    algo.getAlgoValue()));
        }

        final PublicKeyCredentialUserEntity user =
                new PublicKeyCredentialUserEntity(userId,
                        credentialBundle.getString("userName", ""),
                        credentialBundle.getString("userIcon", ""),
                        credentialBundle.getString("userDisplayName", ""));

        AttestationConveyancePreference pref =
                AttestationConveyancePreference.NONE;
        final String attestationPreference =
                authenticatorSelection.getString("attestationPreference", "NONE");
        if (attestationPreference.equalsIgnoreCase(
                AttestationConveyancePreference.DIRECT.name())) {
            pref = AttestationConveyancePreference.DIRECT;
        } else if (attestationPreference.equalsIgnoreCase(
                AttestationConveyancePreference.INDIRECT.name())) {
            pref = AttestationConveyancePreference.INDIRECT;
        }

        final AuthenticatorSelectionCriteria.Builder selBuild =
                new AuthenticatorSelectionCriteria.Builder();
        if (extensions.containsKey("requirePlatformAttachment")) {
            if (authenticatorSelection.getInt("requirePlatformAttachment") == 1) {
                selBuild.setAttachment(Attachment.PLATFORM);
            }
        }
        final AuthenticatorSelectionCriteria sel = selBuild.build();

        final AuthenticationExtensions.Builder extBuilder =
                new AuthenticationExtensions.Builder();
        if (extensions.containsKey("fidoAppId")) {
            extBuilder.setFido2Extension(
                    new FidoAppIdExtension(extensions.getString("fidoAppId")));
        }
        final AuthenticationExtensions ext = extBuilder.build();

        // requireResidentKey andrequireUserVerification are not yet
        // consumed by Android's API

        final List<PublicKeyCredentialDescriptor> excludedList =
                new ArrayList<PublicKeyCredentialDescriptor>();
        for (final WebAuthnTokenManager.WebAuthnPublicCredential cred : excludeList) {
            excludedList.add(
                    new PublicKeyCredentialDescriptor(
                            PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            cred.id,
                            getTransportsForByte(cred.transports)));
        }

        final PublicKeyCredentialRpEntity rp =
                new PublicKeyCredentialRpEntity(
                            credentialBundle.getString("rpId"),
                            credentialBundle.getString("rpName", ""),
                            credentialBundle.getString("rpIcon", ""));

        final PublicKeyCredentialCreationOptions requestOptions =
                requestBuilder
                        .setUser(user)
                        .setAttestationConveyancePreference(pref)
                        .setAuthenticatorSelection(sel)
                        .setAuthenticationExtensions(ext)
                        .setChallenge(challenge)
                        .setRp(rp)
                        .setParameters(params)
                        .setTimeoutSeconds(credentialBundle.getLong("timeoutMS") / 1000.0)
                        .setExcludeList(excludedList)
                        .build();

        final Uri origin = Uri.parse(credentialBundle.getString("origin"));

        final BrowserPublicKeyCredentialCreationOptions browserOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(requestOptions)
                        .setOrigin(origin)
                        .build();

        final Task<PendingIntent> intentTask;

        if (BuildConfig.MOZILLA_OFFICIAL) {
            // Certain Fenix builds and signing keys are whitelisted for Web Authentication.
            // See https://wiki.mozilla.org/Security/Web_Authentication
            //
            // Third party apps will need to get whitelisted themselves.
            final Fido2PrivilegedApiClient fidoClient =
                    Fido.getFido2PrivilegedApiClient(GeckoAppShell.getApplicationContext());

            intentTask = fidoClient.getRegisterPendingIntent(browserOptions);
        } else {
            // For non-official builds, websites have to opt-in to permit the
            // particular version of Gecko to perform WebAuthn operations on
            // them. See https://developers.google.com/digital-asset-links
            // for the general form, and Step 1 of
            // https://developers.google.com/identity/fido/android/native-apps
            // for details about doing this correctly for the FIDO2 API.
            final Fido2ApiClient fidoClient =
                    Fido.getFido2ApiClient(GeckoAppShell.getApplicationContext());

            intentTask = fidoClient.getRegisterPendingIntent(requestOptions);
        }

        final GeckoResult<MakeCredentialResponse> result = new GeckoResult<>();

        intentTask.addOnSuccessListener(pendingIntent -> {
            GeckoRuntime.getInstance().startActivityForResult(pendingIntent).accept(intent -> {
                final WebAuthnTokenManager.Exception error = parseErrorIntent(intent);
                if (error != null) {
                    result.completeExceptionally(error);
                    return;
                }

                final byte[] rspData = intent.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA);
                if (rspData != null) {
                    final AuthenticatorAttestationResponse responseData =
                            AuthenticatorAttestationResponse.deserializeFromBytes(rspData);

                    Log.d(LOGTAG, "key handle: " + Base64.encodeToString(responseData.getKeyHandle(), Base64.DEFAULT));
                    Log.d(LOGTAG, "clientDataJSON: " + Base64.encodeToString(responseData.getClientDataJSON(), Base64.DEFAULT));
                    Log.d(LOGTAG, "attestation Object: " + Base64.encodeToString(responseData.getAttestationObject(), Base64.DEFAULT));

                    result.complete(new WebAuthnTokenManager.MakeCredentialResponse(
                            responseData.getClientDataJSON(),
                            responseData.getKeyHandle(),
                            responseData.getAttestationObject()
                    ));
                }
            }, e -> {
                Log.w(LOGTAG, "Failed to launch activity: ", e);
                Log.w(LOGTAG, "Failed to launch activity: ", e);
                result.completeExceptionally(new WebAuthnTokenManager.Exception("ABORT_ERR"));
            });
        });

        intentTask.addOnFailureListener(e -> {
            Log.w(LOGTAG, "Failed to get FIDO intent", e);
            result.completeExceptionally(new WebAuthnTokenManager.Exception("ABORT_ERR"));
        });

        return result;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void webAuthnMakeCredential(final GeckoBundle credentialBundle,
                                               final ByteBuffer userId,
                                               final ByteBuffer challenge,
                                               final Object[] idList,
                                               final ByteBuffer transportList,
                                               final GeckoBundle authenticatorSelection,
                                               final GeckoBundle extensions) {
        final ArrayList<WebAuthnPublicCredential> excludeList;

        // TODO: Return a GeckoResult instead, Bug 1550116

        final byte[] challBytes = new byte[challenge.remaining()];
        final byte[] userBytes = new byte[userId.remaining()];
        try {
            challenge.get(challBytes);
            userId.get(userBytes);

            excludeList = WebAuthnPublicCredential.CombineBuffers(idList,
                                                                  transportList);
        } catch (final RuntimeException e) {
            Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
            webAuthnMakeCredentialReturnError("UNKNOWN_ERR");
            return;
        }

        try {
            final GeckoResult<MakeCredentialResponse> result = makeCredential(credentialBundle, userBytes, challBytes,
                    excludeList.toArray(new WebAuthnPublicCredential[0]),
                    authenticatorSelection, extensions);
            result.accept(cred -> {
                webAuthnMakeCredentialFinish(cred.clientDataJson, cred.keyHandle, cred.attestationObject);
            }, e -> {
                webAuthnGetAssertionReturnError(e.getMessage());
            });
        } catch (final Exception e) {
            // We need to ensure we catch any possible exception here in order to ensure
            // that the Promise on the content side is appropriately rejected. In particular,
            // we will get `NoClassDefFoundError` if we're running on a device that does not
            // have Google Play Services.
            Log.w(LOGTAG, "Couldn't make credential", e);
            webAuthnMakeCredentialReturnError("UNKNOWN_ERR");
        }
    }

    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnMakeCredentialFinish(final byte[] clientDataJson,
                                                                  final byte[] keyHandle,
                                                                  final byte[] attestationObject);
    @WrapForJNI(dispatchTo = "gecko")
    /* package */ static native void webAuthnMakeCredentialReturnError(String errorCode);

    public static class GetAssertionResponse {
        public final byte[] clientDataJson;
        public final byte[] keyHandle;
        public final byte[] authData;
        public final byte[] signature;
        public final byte[] userHandle;

        public GetAssertionResponse(final byte[] clientDataJson, final byte[] keyHandle,
                                    final byte[] authData, final byte[] signature,
                                    final byte[] userHandle) {
            this.clientDataJson = clientDataJson;
            this.keyHandle = keyHandle;
            this.authData = authData;
            this.signature = signature;
            this.userHandle = userHandle;
        }
    }

    private static WebAuthnTokenManager.Exception parseErrorIntent(final Intent intent) {
        if (!intent.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            return null;
        }

        final byte[] errData = intent.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA);
        final AuthenticatorErrorResponse responseData =
                AuthenticatorErrorResponse.deserializeFromBytes(errData);

        Log.e(LOGTAG, "errorCode.name: " + responseData.getErrorCode());
        Log.e(LOGTAG, "errorMessage: " + responseData.getErrorMessage());


        return new WebAuthnTokenManager.Exception(responseData.getErrorCode().name());
    }

    public static GeckoResult<GetAssertionResponse> getAssertion(final byte[] challenge,
                                                                 final WebAuthnTokenManager.WebAuthnPublicCredential[] allowList,
                                                                 final GeckoBundle assertionBundle,
                                                                 final GeckoBundle extensions) {

        if (!assertionBundle.containsKey("isWebAuthn")) {
            // FIDO U2F not supported by Android (for us anyway) at this time
            return GeckoResult.fromException(new WebAuthnTokenManager.Exception("NOT_SUPPORTED_ERR"));
        }

        final List<PublicKeyCredentialDescriptor> allowedList =
                new ArrayList<PublicKeyCredentialDescriptor>();
        for (final WebAuthnTokenManager.WebAuthnPublicCredential cred : allowList) {
            allowedList.add(
                    new PublicKeyCredentialDescriptor(
                            PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            cred.id,
                            getTransportsForByte(cred.transports)));
        }

        final AuthenticationExtensions.Builder extBuilder =
                new AuthenticationExtensions.Builder();
        if (extensions.containsKey("fidoAppId")) {
            extBuilder.setFido2Extension(
                    new FidoAppIdExtension(extensions.getString("fidoAppId")));
        }
        final AuthenticationExtensions ext = extBuilder.build();

        final PublicKeyCredentialRequestOptions requestOptions =
                new PublicKeyCredentialRequestOptions.Builder()
                        .setChallenge(challenge)
                        .setAllowList(allowedList)
                        .setTimeoutSeconds(assertionBundle.getLong("timeoutMS") / 1000.0)
                        .setRpId(assertionBundle.getString("rpId"))
                        .setAuthenticationExtensions(ext)
                        .build();

        final Uri origin = Uri.parse(assertionBundle.getString("origin"));
        final BrowserPublicKeyCredentialRequestOptions browserOptions =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(requestOptions)
                        .setOrigin(origin)
                        .build();


        final Task<PendingIntent> intentTask;
        // See the makeCredential method for documentation about this
        // conditional.
        if (BuildConfig.MOZILLA_OFFICIAL) {
            final Fido2PrivilegedApiClient fidoClient =
                    Fido.getFido2PrivilegedApiClient(GeckoAppShell.getApplicationContext());

            intentTask = fidoClient.getSignPendingIntent(browserOptions);
        } else {
            final Fido2ApiClient fidoClient =
                    Fido.getFido2ApiClient(GeckoAppShell.getApplicationContext());

            intentTask = fidoClient.getSignPendingIntent(requestOptions);
        }

        final GeckoResult<GetAssertionResponse> result = new GeckoResult<>();
        intentTask.addOnSuccessListener(pendingIntent -> {
            GeckoRuntime.getInstance().startActivityForResult(pendingIntent).accept(intent -> {
                final WebAuthnTokenManager.Exception error = parseErrorIntent(intent);
                if (error != null) {
                    result.completeExceptionally(error);
                    return;
                }

                if (intent.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
                    final byte[] rspData = intent.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA);
                    final AuthenticatorAssertionResponse responseData =
                            AuthenticatorAssertionResponse.deserializeFromBytes(rspData);

                    Log.d(LOGTAG, "key handle: " + Base64.encodeToString(responseData.getKeyHandle(), Base64.DEFAULT));
                    Log.d(LOGTAG, "clientDataJSON: " + Base64.encodeToString(responseData.getClientDataJSON(), Base64.DEFAULT));
                    Log.d(LOGTAG, "auth data: " + Base64.encodeToString(responseData.getAuthenticatorData(), Base64.DEFAULT));
                    Log.d(LOGTAG, "signature: " + Base64.encodeToString(responseData.getSignature(), Base64.DEFAULT));

                    // Nullable field
                    byte[] userHandle = responseData.getUserHandle();
                    if (userHandle == null) {
                        userHandle = new byte[0];
                    }

                    result.complete(new WebAuthnTokenManager.GetAssertionResponse(
                            responseData.getClientDataJSON(),
                            responseData.getKeyHandle(),
                            responseData.getAuthenticatorData(),
                            responseData.getSignature(),
                            userHandle));
                }
            }, e -> {
                Log.w(LOGTAG, "Failed to get FIDO intent", e);
                result.completeExceptionally(new WebAuthnTokenManager.Exception("UNKNOWN_ERR"));
            });
        });

        return result;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void webAuthnGetAssertion(final ByteBuffer challenge,
                                             final Object[] idList,
                                             final ByteBuffer transportList,
                                             final GeckoBundle assertionBundle,
                                             final GeckoBundle extensions) {
        final ArrayList<WebAuthnPublicCredential> allowList;

        // TODO: Return a GeckoResult instead, Bug 1550116

        final byte[] challBytes = new byte[challenge.remaining()];
        try {
            challenge.get(challBytes);
            allowList = WebAuthnPublicCredential.CombineBuffers(idList,
                                                                transportList);
        } catch (final RuntimeException e) {
            Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
            webAuthnGetAssertionReturnError("UNKNOWN_ERR");
            return;
        }

        try {
            getAssertion(challBytes,
                    allowList.toArray(new WebAuthnPublicCredential[0]),
                    assertionBundle, extensions).accept(response -> {
                        webAuthnGetAssertionFinish(response.clientDataJson, response.keyHandle, response.authData,
                                response.signature, response.userHandle);
                    }, e -> {
                        webAuthnGetAssertionReturnError(e.getMessage());
                    });
        } catch (final java.lang.Exception e) {
            Log.w(LOGTAG, "Couldn't get assertion", e);
            webAuthnGetAssertionReturnError("UNKNOWN_ERR");
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

    @WrapForJNI(calledFrom = "gecko")
    private static GeckoResult<Boolean> webAuthnIsUserVerifyingPlatformAuthenticatorAvailable() {
        final Task<Boolean> task;
        if (BuildConfig.MOZILLA_OFFICIAL) {
            final Fido2PrivilegedApiClient fidoClient =
                    Fido.getFido2PrivilegedApiClient(GeckoAppShell.getApplicationContext());
            task = fidoClient.isUserVerifyingPlatformAuthenticatorAvailable();
        } else {
            final Fido2ApiClient fidoClient =
                    Fido.getFido2ApiClient(GeckoAppShell.getApplicationContext());
            task = fidoClient.isUserVerifyingPlatformAuthenticatorAvailable();
        }

        final GeckoResult<Boolean> res = new GeckoResult<>();
        task.addOnSuccessListener(isUVPAA -> {
            res.complete(isUVPAA);
        });
        task.addOnFailureListener(e -> {
            Log.w(LOGTAG, "isUserVerifyingPlatformAuthenticatorAvailable is failed", e);
            res.complete(false);
        });
        return res;
    }
}
