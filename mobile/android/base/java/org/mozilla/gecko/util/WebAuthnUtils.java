/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.WebAuthnTokenManager;
import org.mozilla.gecko.GeckoActivityMonitor;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.GeckoBundle;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.net.Uri;
import android.util.Log;
import android.util.Base64;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.common.Transport;
import com.google.android.gms.fido.fido2.Fido2PendingIntent;
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
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.OnFailureListener;

public class WebAuthnUtils
{
    private static final String LOG_TAG = "WebAuthnUtils";

    // from u2fhid-capi.h
    private static final byte AUTHENTICATOR_TRANSPORT_USB = 1;
    private static final byte AUTHENTICATOR_TRANSPORT_NFC = 2;
    private static final byte AUTHENTICATOR_TRANSPORT_BLE = 4;

    private static List<Transport> getTransportsForByte(final byte transports) {
        ArrayList<Transport> result = new ArrayList<Transport>();
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

    @ReflectionTarget
    public static void makeCredential(final GeckoBundle credentialBundle,
                                      final byte[] userId, final byte[] challenge,
                                      final WebAuthnTokenManager.WebAuthnPublicCredential[] excludeList,
                                      final GeckoBundle authenticatorSelection,
                                      final GeckoBundle extensions,
                                      WebAuthnTokenManager.WebAuthnMakeCredentialResponse handler) {
        final Activity currentActivity =
            GeckoActivityMonitor.getInstance().getCurrentActivity();

        if (currentActivity == null) {
            handler.onFailure("UNKNOWN_ERR");
            return;
        }

        if (!credentialBundle.containsKey("isWebAuthn")) {
            // FIDO U2F not supported by Android (for us anyway) at this time
            handler.onFailure("NOT_SUPPORTED_ERR");
            return;
        }

        PublicKeyCredentialCreationOptions.Builder requestBuilder =
            new PublicKeyCredentialCreationOptions.Builder();

        List<PublicKeyCredentialParameters> params =
            new ArrayList<PublicKeyCredentialParameters>();

        // WebAuthn suuports more algorithms
        for (Algorithm algo : new Algorithm[]{
                EC2Algorithm.ES256, EC2Algorithm.ES384, EC2Algorithm.ES512,
                EC2Algorithm.ED256, /* no ED384 */      EC2Algorithm.ED512,
                RSAAlgorithm.PS256, RSAAlgorithm.PS384, RSAAlgorithm.PS512,
                RSAAlgorithm.RS256, RSAAlgorithm.RS384, RSAAlgorithm.RS512
            }) {
            params.add(new PublicKeyCredentialParameters(
                PublicKeyCredentialType.PUBLIC_KEY.toString(),
                algo.getAlgoValue()));
        }

        PublicKeyCredentialUserEntity user =
            new PublicKeyCredentialUserEntity(userId,
                    credentialBundle.getString("userName", ""),
                    credentialBundle.getString("userIcon", ""),
                    credentialBundle.getString("userDisplayName", ""));

        AttestationConveyancePreference pref =
            AttestationConveyancePreference.NONE;
        String attestationPreference =
            authenticatorSelection.getString("attestationPreference", "NONE");
        if (attestationPreference.equalsIgnoreCase(
                              AttestationConveyancePreference.DIRECT.name())) {
            pref = AttestationConveyancePreference.DIRECT;
        } else if (attestationPreference.equalsIgnoreCase(
                            AttestationConveyancePreference.INDIRECT.name())) {
            pref = AttestationConveyancePreference.INDIRECT;
        }

        AuthenticatorSelectionCriteria.Builder selBuild =
            new AuthenticatorSelectionCriteria.Builder();
        if (extensions.containsKey("requirePlatformAttachment")) {
            if (authenticatorSelection.getInt("requirePlatformAttachment") == 1) {
                selBuild.setAttachment(Attachment.PLATFORM);
            }
        }
        AuthenticatorSelectionCriteria sel = selBuild.build();

        AuthenticationExtensions.Builder extBuilder =
            new AuthenticationExtensions.Builder();
        if (extensions.containsKey("fidoAppId")) {
            extBuilder.setFido2Extension(
                new FidoAppIdExtension(extensions.getString("fidoAppId")));
        }
        AuthenticationExtensions ext = extBuilder.build();

        // requireResidentKey andrequireUserVerification are not yet
        // consumed by Android's API

        List<PublicKeyCredentialDescriptor> excludedList =
            new ArrayList<PublicKeyCredentialDescriptor>();
        for (WebAuthnTokenManager.WebAuthnPublicCredential cred : excludeList) {
            excludedList.add(
                new PublicKeyCredentialDescriptor(
                                    PublicKeyCredentialType.PUBLIC_KEY.toString(),
                                    cred.mId,
                                    getTransportsForByte(cred.mTransports)));
        }

        PublicKeyCredentialRpEntity rp =
            new PublicKeyCredentialRpEntity(
                    credentialBundle.getString("rpId"),
                    credentialBundle.getString("rpName", ""),
                    credentialBundle.getString("rpIcon", ""));

        PublicKeyCredentialCreationOptions requestOptions =
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

        Uri origin = Uri.parse(credentialBundle.getString("origin"));

        BrowserPublicKeyCredentialCreationOptions browserOptions =
            new BrowserPublicKeyCredentialCreationOptions.Builder()
                .setPublicKeyCredentialCreationOptions(requestOptions)
                .setOrigin(origin)
                .build();

        Task<Fido2PendingIntent> result;

        if (AppConstants.MOZILLA_OFFICIAL) {
            // The privileged API only works in released builds, signed by
            // Mozilla infrastructure. This permits setting the origin to a
            // webpage one.
            Fido2PrivilegedApiClient fidoClient =
                Fido.getFido2PrivilegedApiClient(currentActivity.getApplicationContext());

            result = fidoClient.getRegisterIntent(browserOptions);
        } else {
            // For non-official builds, websites have to opt-in to permit the
            // particular version of Gecko to perform WebAuthn operations on
            // them. See https://developers.google.com/digital-asset-links
            // for the general form, and Step 1 of
            // https://developers.google.com/identity/fido/android/native-apps
            // for details about doing this correctly for the FIDO2 API.
            Fido2ApiClient fidoClient =
                Fido.getFido2ApiClient(currentActivity.getApplicationContext());

            result = fidoClient.getRegisterIntent(requestOptions);
        }

        result.addOnSuccessListener(new OnSuccessListener<Fido2PendingIntent>() {
            @Override
            public void onSuccess(Fido2PendingIntent pendingIntent) {
                if (pendingIntent.hasPendingIntent()) {
                    final WebAuthnMakeCredentialResult resultHandler =
                        new WebAuthnMakeCredentialResult(handler);

                    try {
                        pendingIntent.launchPendingIntent(currentActivity,
                            ActivityHandlerHelper.registerActivityHandler(resultHandler));
                    } catch (IntentSender.SendIntentException e) {
                        handler.onFailure("UNKNOWN_ERR");
                    }
                }
            }
        });
        result.addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(Exception e) {
                    Log.w(LOG_TAG, "onFailure=" + e);
                    e.printStackTrace();
                    handler.onFailure("UNKNOWN_ERR");
                }
        });
    }

    private static class WebAuthnMakeCredentialResult implements ActivityResultHandler {
        private WebAuthnTokenManager.WebAuthnMakeCredentialResponse mHandler;

        WebAuthnMakeCredentialResult(WebAuthnTokenManager.WebAuthnMakeCredentialResponse handler) {
            this.mHandler = handler;
        }

        @Override
        public void onActivityResult(final int resultCode, final Intent data) {
            if (resultCode == Activity.RESULT_OK) {
                if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
                    byte[] errData = data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA);
                    AuthenticatorErrorResponse responseData =
                        AuthenticatorErrorResponse.deserializeFromBytes(errData);

                    Log.e(LOG_TAG, "errorCode.name: " + responseData.getErrorCode());
                    Log.e(LOG_TAG, "errorMessage: " + responseData.getErrorMessage());

                    mHandler.onFailure(responseData.getErrorCode().name());
                    return;
                }

                if (data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
                    byte[] rspData = data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA);
                    AuthenticatorAttestationResponse responseData =
                        AuthenticatorAttestationResponse.deserializeFromBytes(rspData);

                    Log.e(LOG_TAG, "key handle: " + Base64.encodeToString(responseData.getKeyHandle(), Base64.DEFAULT));
                    Log.e(LOG_TAG, "clientDataJSON: " + Base64.encodeToString(responseData.getClientDataJSON(), Base64.DEFAULT));
                    Log.e(LOG_TAG, "attestation Object: " + Base64.encodeToString(responseData.getAttestationObject(), Base64.DEFAULT));

                    mHandler.onSuccess(
                        responseData.getClientDataJSON(),
                        responseData.getKeyHandle(),
                        responseData.getAttestationObject());
                    return;
                }
            }

            if (resultCode == Activity.RESULT_CANCELED) {
                Log.w(LOG_TAG, "RESULT_CANCELED" + resultCode);
                mHandler.onFailure("ABORT_ERR");
                return;
            }

            mHandler.onFailure("UNKNOWN_ERR");
        }
    }

    @ReflectionTarget
    public static void getAssertion(final byte[] challenge,
                                    final WebAuthnTokenManager.WebAuthnPublicCredential[] allowList,
                                    final GeckoBundle assertionBundle,
                                    final GeckoBundle extensions,
                                    WebAuthnTokenManager.WebAuthnGetAssertionResponse handler) {
        final Activity currentActivity =
            GeckoActivityMonitor.getInstance().getCurrentActivity();

        if (currentActivity == null) {
            handler.onFailure("UNKNOWN_ERR");
            return;
        }

        if (!assertionBundle.containsKey("isWebAuthn")) {
            // FIDO U2F not supported by Android (for us anyway) at this time
            handler.onFailure("NOT_SUPPORTED_ERR");
            return;
        }

        List<PublicKeyCredentialDescriptor> allowedList =
            new ArrayList<PublicKeyCredentialDescriptor>();
        for (WebAuthnTokenManager.WebAuthnPublicCredential cred : allowList) {
            allowedList.add(
                new PublicKeyCredentialDescriptor(
                                    PublicKeyCredentialType.PUBLIC_KEY.toString(),
                                    cred.mId,
                                    getTransportsForByte(cred.mTransports)));
        }

        AuthenticationExtensions.Builder extBuilder =
            new AuthenticationExtensions.Builder();
        if (extensions.containsKey("fidoAppId")) {
            extBuilder.setFido2Extension(
                new FidoAppIdExtension(extensions.getString("fidoAppId")));
        }
        AuthenticationExtensions ext = extBuilder.build();

        PublicKeyCredentialRequestOptions requestOptions =
            new PublicKeyCredentialRequestOptions.Builder()
                .setChallenge(challenge)
                .setAllowList(allowedList)
                .setTimeoutSeconds(assertionBundle.getLong("timeoutMS") / 1000.0)
                .setRpId(assertionBundle.getString("rpId"))
                .setAuthenticationExtensions(ext)
                .build();

        Uri origin = Uri.parse(assertionBundle.getString("origin"));
        BrowserPublicKeyCredentialRequestOptions browserOptions =
            new BrowserPublicKeyCredentialRequestOptions.Builder()
                .setPublicKeyCredentialRequestOptions(requestOptions)
                .setOrigin(origin)
                .build();


        Task<Fido2PendingIntent> result;
        // See the makeCredential method for documentation about this
        // conditional.
        if (AppConstants.MOZILLA_OFFICIAL) {
            Fido2PrivilegedApiClient fidoClient =
                Fido.getFido2PrivilegedApiClient(currentActivity.getApplicationContext());

            result = fidoClient.getSignIntent(browserOptions);
        } else {
            Fido2ApiClient fidoClient =
                Fido.getFido2ApiClient(currentActivity.getApplicationContext());

            result = fidoClient.getSignIntent(requestOptions);
        }

        result.addOnSuccessListener(new OnSuccessListener<Fido2PendingIntent>() {
            @Override
            public void onSuccess(Fido2PendingIntent pendingIntent) {
                if (pendingIntent.hasPendingIntent()) {
                    final WebAuthnGetAssertionResult resultHandler =
                        new WebAuthnGetAssertionResult(handler);

                    try {
                        pendingIntent.launchPendingIntent(currentActivity,
                            ActivityHandlerHelper.registerActivityHandler(resultHandler));
                    } catch (IntentSender.SendIntentException e) {
                        Log.w(LOG_TAG, "pendingIntent failure", e);
                        handler.onFailure("UNKNOWN_ERR");
                    }
                }
            }
        });
        result.addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(Exception e) {
                    Log.w(LOG_TAG, "onFailure=" + e);
                    e.printStackTrace();
                    handler.onFailure("UNKNOWN_ERR");
                }
        });
    }

    private static class WebAuthnGetAssertionResult implements ActivityResultHandler {
        private WebAuthnTokenManager.WebAuthnGetAssertionResponse mHandler;

        WebAuthnGetAssertionResult(WebAuthnTokenManager.WebAuthnGetAssertionResponse handler) {
            this.mHandler = handler;
        }

        @Override
        public void onActivityResult(final int resultCode, Intent data) {
            if (resultCode == Activity.RESULT_OK) {

                if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
                    Log.w(LOG_TAG, "FIDO2_KEY_ERROR_EXTRA and right");
                    byte[] errData = data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA);
                    AuthenticatorErrorResponse responseData =
                        AuthenticatorErrorResponse.deserializeFromBytes(errData);

                    Log.e(LOG_TAG, "errorCode.name: " + responseData.getErrorCode());
                    Log.e(LOG_TAG, "errorMessage: " + responseData.getErrorMessage());

                    mHandler.onFailure(responseData.getErrorCode().name());
                    return;
                }

                if (data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
                    Log.w(LOG_TAG, "FIDO2_KEY_RESPONSE_EXTRA and right");
                    byte[] rspData = data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA);
                    AuthenticatorAssertionResponse responseData =
                        AuthenticatorAssertionResponse.deserializeFromBytes(rspData);

                    Log.e(LOG_TAG, "key handle: " + Base64.encodeToString(responseData.getKeyHandle(), Base64.DEFAULT));
                    Log.e(LOG_TAG, "clientDataJSON: " + Base64.encodeToString(responseData.getClientDataJSON(), Base64.DEFAULT));
                    Log.e(LOG_TAG, "auth data: " + Base64.encodeToString(responseData.getAuthenticatorData(), Base64.DEFAULT));
                    Log.e(LOG_TAG, "signature: " + Base64.encodeToString(responseData.getSignature(), Base64.DEFAULT));

                    // Nullable field
                    byte[] userHandle = responseData.getUserHandle();
                    if (userHandle == null) {
                        userHandle = new byte[0];
                    }

                    mHandler.onSuccess(
                        responseData.getClientDataJSON(),
                        responseData.getKeyHandle(),
                        responseData.getAuthenticatorData(),
                        responseData.getSignature(),
                        userHandle);
                    return;
                }
            }

            if (resultCode == Activity.RESULT_CANCELED) {
                Log.w(LOG_TAG, "RESULT_CANCELED" + resultCode);
                mHandler.onFailure("ABORT_ERR");
                return;
            }

            mHandler.onFailure("UNKNOWN_ERR");
        }
    }


}
