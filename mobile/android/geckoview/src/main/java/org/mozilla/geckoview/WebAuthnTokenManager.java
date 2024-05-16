/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.app.PendingIntent;
import android.net.Uri;
import android.os.Build;
import android.util.Log;
import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.fido2.Fido2ApiClient;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.Algorithm;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticationExtensions;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.EC2Algorithm;
import com.google.android.gms.fido.fido2.api.common.FidoAppIdExtension;
import com.google.android.gms.fido.fido2.api.common.FidoCredentialDetails;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRpEntity;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialType;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialUserEntity;
import com.google.android.gms.fido.fido2.api.common.RSAAlgorithm;
import com.google.android.gms.fido.fido2.api.common.ResidentKeyRequirement;
import com.google.android.gms.tasks.Task;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.WebAuthnCredentialManager;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.WebAuthnUtils;

/* package */ class WebAuthnTokenManager {
  private static final String LOGTAG = "WebAuthnTokenManager";
  private static final boolean DEBUG = false;

  private static final Algorithm[] SUPPORTED_ALGORITHMS = {
    EC2Algorithm.ES256,
    EC2Algorithm.ES384,
    EC2Algorithm.ES512,
    EC2Algorithm.ED256, /* no ED384 */
    EC2Algorithm.ED512,
    RSAAlgorithm.PS256,
    RSAAlgorithm.PS384,
    RSAAlgorithm.PS512,
    RSAAlgorithm.RS256,
    RSAAlgorithm.RS384,
    RSAAlgorithm.RS512
  };

  private static PublicKeyCredentialCreationOptions getRequestOptionsForMakeCredential(
      final GeckoBundle credentialBundle,
      final byte[] userId,
      final byte[] challenge,
      final WebAuthnUtils.WebAuthnPublicCredential[] excludeList,
      final GeckoBundle authenticatorSelection,
      final GeckoBundle extensions) {
    final PublicKeyCredentialCreationOptions.Builder requestBuilder =
        new PublicKeyCredentialCreationOptions.Builder();

    final List<PublicKeyCredentialParameters> params =
        new ArrayList<PublicKeyCredentialParameters>();

    // WebAuthn supports more algorithms
    for (final Algorithm algo : SUPPORTED_ALGORITHMS) {
      params.add(
          new PublicKeyCredentialParameters(
              PublicKeyCredentialType.PUBLIC_KEY.toString(), algo.getAlgoValue()));
    }

    final GeckoBundle userBundle = credentialBundle.getBundle("user");
    final PublicKeyCredentialUserEntity user =
        new PublicKeyCredentialUserEntity(
            userId,
            userBundle.getString("name", ""),
            /* deprecated userIcon field */ "",
            userBundle.getString("displayName", ""));

    AttestationConveyancePreference pref = AttestationConveyancePreference.NONE;
    final String attestationPreference = credentialBundle.getString("attestation", "NONE");
    if (attestationPreference.equalsIgnoreCase(AttestationConveyancePreference.DIRECT.name())) {
      pref = AttestationConveyancePreference.DIRECT;
    } else if (attestationPreference.equalsIgnoreCase(
        AttestationConveyancePreference.INDIRECT.name())) {
      pref = AttestationConveyancePreference.INDIRECT;
    }

    final AuthenticatorSelectionCriteria.Builder selBuild =
        new AuthenticatorSelectionCriteria.Builder();
    final String authenticatorAttachment =
        authenticatorSelection.getString("authenticatorAttachment", "");
    if (authenticatorAttachment.equals("platform")) {
      selBuild.setAttachment(Attachment.PLATFORM);
    } else if (authenticatorAttachment.equals("cross-platform")) {
      selBuild.setAttachment(Attachment.CROSS_PLATFORM);
    }
    final String residentKey = authenticatorSelection.getString("residentKey", "");
    if (residentKey.equals("required")) {
      selBuild
          .setRequireResidentKey(true)
          .setResidentKeyRequirement(ResidentKeyRequirement.RESIDENT_KEY_REQUIRED);
    } else if (residentKey.equals("preferred")) {
      selBuild
          .setRequireResidentKey(false)
          .setResidentKeyRequirement(ResidentKeyRequirement.RESIDENT_KEY_PREFERRED);
    } else if (residentKey.equals("discouraged")) {
      selBuild
          .setRequireResidentKey(false)
          .setResidentKeyRequirement(ResidentKeyRequirement.RESIDENT_KEY_DISCOURAGED);
    }
    final AuthenticatorSelectionCriteria sel = selBuild.build();

    final AuthenticationExtensions.Builder extBuilder = new AuthenticationExtensions.Builder();
    if (extensions.containsKey("fidoAppId")) {
      extBuilder.setFido2Extension(new FidoAppIdExtension(extensions.getString("fidoAppId")));
    }
    final AuthenticationExtensions ext = extBuilder.build();

    // requireUserVerification are not yet consumed by Android's API

    final List<PublicKeyCredentialDescriptor> excludedList =
        new ArrayList<PublicKeyCredentialDescriptor>();
    for (final WebAuthnUtils.WebAuthnPublicCredential cred : excludeList) {
      excludedList.add(
          new PublicKeyCredentialDescriptor(
              PublicKeyCredentialType.PUBLIC_KEY.toString(),
              cred.id,
              WebAuthnUtils.getTransportsForByte(cred.transports)));
    }

    final GeckoBundle rpBundle = credentialBundle.getBundle("rp");
    final PublicKeyCredentialRpEntity rp =
        new PublicKeyCredentialRpEntity(
            rpBundle.getString("id"),
            rpBundle.getString("name", ""),
            /* deprecated rpIcon field */ "");

    return requestBuilder
        .setUser(user)
        .setAttestationConveyancePreference(pref)
        .setAuthenticatorSelection(sel)
        .setAuthenticationExtensions(ext)
        .setChallenge(challenge)
        .setRp(rp)
        .setParameters(params)
        .setTimeoutSeconds(credentialBundle.getLong("timeout") / 1000.0)
        .setExcludeList(excludedList)
        .build();
  }

  public static GeckoResult<WebAuthnUtils.MakeCredentialResponse> makeCredential(
      final GeckoBundle credentialBundle,
      final byte[] userId,
      final byte[] challenge,
      final WebAuthnUtils.WebAuthnPublicCredential[] excludeList,
      final GeckoBundle authenticatorSelection,
      final GeckoBundle extensions) {
    if (!credentialBundle.containsKey("isWebAuthn")) {
      // FIDO U2F not supported by Android (for us anyway) at this time
      return GeckoResult.fromException(new WebAuthnUtils.Exception("NOT_SUPPORTED_ERR"));
    }

    final GeckoResult<WebAuthnUtils.MakeCredentialResponse> result = new GeckoResult<>();
    try {
      final PublicKeyCredentialCreationOptions requestOptions =
          getRequestOptionsForMakeCredential(
              credentialBundle, userId, challenge, excludeList, authenticatorSelection, extensions);

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

      intentTask.addOnSuccessListener(
          pendingIntent -> {
            GeckoRuntime.getInstance()
                .startActivityForResult(pendingIntent)
                .accept(
                    intent -> {
                      if (!intent.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
                        Log.w(LOGTAG, "Failed to get credential data in FIDO intent");
                        result.completeExceptionally(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
                        return;
                      }
                      final byte[] rspData =
                          intent.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA);
                      final PublicKeyCredential publicKeyCredentialData =
                          PublicKeyCredential.deserializeFromBytes(rspData);

                      final AuthenticatorResponse response = publicKeyCredentialData.getResponse();
                      final WebAuthnUtils.Exception error = parseErrorResponse(response);
                      if (error != null) {
                        result.completeExceptionally(error);
                        return;
                      }

                      if (!(response instanceof AuthenticatorAttestationResponse)) {
                        Log.w(LOGTAG, "Failed to get attestation response in FIDO intent");
                        result.completeExceptionally(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
                        return;
                      }

                      final AuthenticatorAttestationResponse responseData =
                          (AuthenticatorAttestationResponse) response;
                      final WebAuthnUtils.MakeCredentialResponse credentialResponse =
                          new WebAuthnUtils.MakeCredentialResponse.Builder()
                              .setKeyHandle(publicKeyCredentialData.getRawId())
                              .setAuthenticatorAttachment(
                                  publicKeyCredentialData.getAuthenticatorAttachment())
                              .setClientDataJson(responseData.getClientDataJSON())
                              .setAttestationObject(responseData.getAttestationObject())
                              .setTransports(responseData.getTransports())
                              .build();

                      Log.d(LOGTAG, "MakeCredentialResponse: " + credentialResponse.toString());
                      result.complete(credentialResponse);
                    },
                    e -> {
                      Log.w(LOGTAG, "Failed to launch activity: ", e);
                      result.completeExceptionally(new WebAuthnUtils.Exception("ABORT_ERR"));
                    });
          });

      intentTask.addOnFailureListener(
          e -> {
            Log.w(LOGTAG, "Failed to get FIDO intent", e);
            result.completeExceptionally(new WebAuthnUtils.Exception("ABORT_ERR"));
          });
    } catch (final java.lang.Exception e) {
      // We need to ensure we catch any possible exception here in order to ensure
      // that the Promise on the content side is appropriately rejected. In particular,
      // we will get `NoClassDefFoundError` if we're running on a device that does not
      // have Google Play Services.
      Log.w(LOGTAG, "Couldn't make credential", e);
      return GeckoResult.fromException(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
    }
    return result;
  }

  @WrapForJNI(calledFrom = "gecko")
  private static GeckoResult<WebAuthnUtils.MakeCredentialResponse> webAuthnMakeCredential(
      final GeckoBundle credentialBundle,
      final ByteBuffer userId,
      final ByteBuffer challenge,
      final Object[] idList,
      final ByteBuffer transportList,
      final GeckoBundle authenticatorSelection,
      final GeckoBundle extensions,
      final int[] algs,
      final ByteBuffer clientDataHash) {
    final ArrayList<WebAuthnUtils.WebAuthnPublicCredential> excludeArrayList;

    final byte[] challBytes = new byte[challenge.remaining()];
    final byte[] userBytes = new byte[userId.remaining()];
    final byte[] clientDataHashBytes = new byte[clientDataHash.remaining()];
    try {
      challenge.get(challBytes);
      userId.get(userBytes);
      clientDataHash.get(clientDataHashBytes);

      excludeArrayList =
          WebAuthnUtils.WebAuthnPublicCredential.CombineBuffers(idList, transportList);
    } catch (final RuntimeException e) {
      Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
      return GeckoResult.fromException(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
    }

    final WebAuthnUtils.WebAuthnPublicCredential[] excludeList =
        new WebAuthnUtils.WebAuthnPublicCredential[idList.length];
    excludeArrayList.toArray(excludeList);

    final GeckoResult<WebAuthnUtils.MakeCredentialResponse> result = new GeckoResult<>();
    WebAuthnCredentialManager.makeCredential(
            credentialBundle,
            userBytes,
            challBytes,
            algs,
            excludeList,
            authenticatorSelection,
            clientDataHashBytes)
        .accept(
            response -> result.complete(response),
            exceptionInCredManager -> {
              if (!exceptionInCredManager.getMessage().equals("NOT_SUPPORTED_ERR")) {
                result.completeExceptionally(exceptionInCredManager);
                return;
              }
              // If supported error, we try to use GMS FIDO2.
              makeCredential(
                      credentialBundle,
                      userBytes,
                      challBytes,
                      excludeList,
                      authenticatorSelection,
                      extensions)
                  .accept(
                      response -> result.complete(response),
                      exceptionInGMS -> result.completeExceptionally(exceptionInGMS));
            });
    return result;
  }

  private static WebAuthnUtils.Exception parseErrorResponse(final AuthenticatorResponse response) {
    if (!(response instanceof AuthenticatorErrorResponse)) {
      return null;
    }

    final AuthenticatorErrorResponse responseData = (AuthenticatorErrorResponse) response;

    Log.e(LOGTAG, "errorCode.name: " + responseData.getErrorCode());
    Log.e(LOGTAG, "errorMessage: " + responseData.getErrorMessage());

    return new WebAuthnUtils.Exception(responseData.getErrorCode().name());
  }

  private static PublicKeyCredentialRequestOptions getRequestOptionsForGetAssertion(
      final byte[] challenge,
      final WebAuthnUtils.WebAuthnPublicCredential[] allowList,
      final GeckoBundle assertionBundle,
      final GeckoBundle extensions) {
    final List<PublicKeyCredentialDescriptor> allowedList =
        new ArrayList<PublicKeyCredentialDescriptor>();
    for (final WebAuthnUtils.WebAuthnPublicCredential cred : allowList) {
      allowedList.add(
          new PublicKeyCredentialDescriptor(
              PublicKeyCredentialType.PUBLIC_KEY.toString(),
              cred.id,
              WebAuthnUtils.getTransportsForByte(cred.transports)));
    }

    final AuthenticationExtensions.Builder extBuilder = new AuthenticationExtensions.Builder();
    if (extensions.containsKey("fidoAppId")) {
      extBuilder.setFido2Extension(new FidoAppIdExtension(extensions.getString("fidoAppId")));
    }
    final AuthenticationExtensions ext = extBuilder.build();

    return new PublicKeyCredentialRequestOptions.Builder()
        .setChallenge(challenge)
        .setAllowList(allowedList)
        .setTimeoutSeconds(assertionBundle.getLong("timeout") / 1000.0)
        .setRpId(assertionBundle.getString("rpId"))
        .setAuthenticationExtensions(ext)
        .build();
  }

  private static GeckoResult<WebAuthnUtils.GetAssertionResponse> getAssertion(
      final byte[] challenge,
      final WebAuthnUtils.WebAuthnPublicCredential[] allowList,
      final GeckoBundle assertionBundle,
      final GeckoBundle extensions) {

    if (!assertionBundle.containsKey("isWebAuthn")) {
      // FIDO U2F not supported by Android (for us anyway) at this time
      return GeckoResult.fromException(new WebAuthnUtils.Exception("NOT_SUPPORTED_ERR"));
    }

    final GeckoResult<WebAuthnUtils.GetAssertionResponse> result = new GeckoResult<>();
    try {
      final PublicKeyCredentialRequestOptions requestOptions =
          getRequestOptionsForGetAssertion(challenge, allowList, assertionBundle, extensions);

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

      intentTask.addOnSuccessListener(
          pendingIntent -> {
            GeckoRuntime.getInstance()
                .startActivityForResult(pendingIntent)
                .accept(
                    intent -> {
                      if (!intent.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
                        Log.w(LOGTAG, "Failed to get credential data in FIDO intent");
                        result.completeExceptionally(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
                        return;
                      }

                      final byte[] rspData =
                          intent.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA);
                      final PublicKeyCredential publicKeyCredentialData =
                          PublicKeyCredential.deserializeFromBytes(rspData);
                      final AuthenticatorResponse response = publicKeyCredentialData.getResponse();

                      final WebAuthnUtils.Exception error = parseErrorResponse(response);
                      if (error != null) {
                        result.completeExceptionally(error);
                        return;
                      }

                      if (!(response instanceof AuthenticatorAssertionResponse)) {
                        Log.w(LOGTAG, "Failed to get assertion response in FIDO intent");
                        result.completeExceptionally(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
                        return;
                      }

                      final AuthenticatorAssertionResponse responseData =
                          (AuthenticatorAssertionResponse) response;
                      final WebAuthnUtils.GetAssertionResponse assertionResponse =
                          new WebAuthnUtils.GetAssertionResponse.Builder()
                              .setClientDataJson(responseData.getClientDataJSON())
                              .setKeyHandle(publicKeyCredentialData.getRawId())
                              .setAuthData(responseData.getAuthenticatorData())
                              .setSignature(responseData.getSignature())
                              .setUserHandle(responseData.getUserHandle())
                              .setAuthenticatorAttachment(
                                  publicKeyCredentialData.getAuthenticatorAttachment())
                              .build();
                      Log.d(LOGTAG, "GetAssertionResponse: " + assertionResponse.toString());
                      result.complete(assertionResponse);
                    },
                    e -> {
                      Log.w(LOGTAG, "Failed to get FIDO intent", e);
                      result.completeExceptionally(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
                    });
          });
    } catch (final java.lang.Exception e) {
      // We need to ensure we catch any possible exception here in order to ensure
      // that the Promise on the content side is appropriately rejected. In particular,
      // we will get `NoClassDefFoundError` if we're running on a device that does not
      // have Google Play Services.
      Log.w(LOGTAG, "Couldn't make credential", e);
      return GeckoResult.fromException(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
    }
    return result;
  }

  @WrapForJNI(calledFrom = "gecko")
  private static GeckoResult<WebAuthnUtils.GetAssertionResponse> webAuthnGetAssertion(
      final ByteBuffer challenge,
      final Object[] idList,
      final ByteBuffer transportList,
      final GeckoBundle assertionBundle,
      final GeckoBundle extensions,
      final ByteBuffer clientDataHash) {
    final ArrayList<WebAuthnUtils.WebAuthnPublicCredential> allowArrayList;

    final byte[] challBytes = new byte[challenge.remaining()];
    final byte[] clientDataHashBytes = new byte[clientDataHash.remaining()];
    try {
      challenge.get(challBytes);
      clientDataHash.get(clientDataHashBytes);
      allowArrayList = WebAuthnUtils.WebAuthnPublicCredential.CombineBuffers(idList, transportList);
    } catch (final RuntimeException e) {
      Log.w(LOGTAG, "Couldn't extract nio byte arrays!", e);
      return GeckoResult.fromException(new WebAuthnUtils.Exception("UNKNOWN_ERR"));
    }

    final WebAuthnUtils.WebAuthnPublicCredential[] allowList =
        new WebAuthnUtils.WebAuthnPublicCredential[idList.length];
    allowArrayList.toArray(allowList);

    // Unfortunately, we don't know where requested credential is stored.
    //
    // 1. Check whether play service has the credential that is included in allow list.
    //    If found, use play service. If not, step 2.
    // 2. Try credential manager to get credential.
    //    If credential manager doesn't support passkeys, step 3.
    // 3. Try play service to get credential.
    final GeckoResult<WebAuthnUtils.GetAssertionResponse> result = new GeckoResult<>();
    hasCredentialInGMS(assertionBundle.getString("rpId"), allowList)
        .accept(
            found -> {
              if (found) {
                // Use GMS API due to having credential, or older Android
                getAssertion(challBytes, allowList, assertionBundle, extensions)
                    .accept(
                        response -> result.complete(response),
                        e -> result.completeExceptionally(e));
                return;
              }

              WebAuthnCredentialManager.prepareGetAssertion(
                      challBytes, allowList, assertionBundle, extensions, clientDataHashBytes)
                  .accept(
                      pendingHandle -> {
                        if (pendingHandle != null) {
                          WebAuthnCredentialManager.getAssertion(pendingHandle)
                              .accept(
                                  response -> result.complete(response),
                                  e -> result.completeExceptionally(e));
                          return;
                        }
                        // Maybe credential manager has no credentials
                        getAssertion(challBytes, allowList, assertionBundle, extensions)
                            .accept(
                                response -> result.complete(response),
                                e -> result.completeExceptionally(e));
                      },
                      e -> result.completeExceptionally(e));
              return;
            });
    return result;
  }

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
    task.addOnSuccessListener(
        isUVPAA -> {
          res.complete(isUVPAA);
        });
    task.addOnFailureListener(
        e -> {
          Log.w(LOGTAG, "isUserVerifyingPlatformAuthenticatorAvailable is failed", e);
          res.complete(false);
        });
    return res;
  }

  private static GeckoResult<Boolean> hasCredentialInGMS(
      final String rpId, final WebAuthnUtils.WebAuthnPublicCredential[] allowList) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
      // Always use GMS on old Android
      return GeckoResult.fromValue(true);
    }
    if (!BuildConfig.MOZILLA_OFFICIAL) {
      // No way to check credential in GMS. Use Credential Manager at first.
      return GeckoResult.fromValue(false);
    }

    final GeckoResult<Boolean> result = new GeckoResult<>();
    try {
      final Fido2PrivilegedApiClient fidoClient =
          Fido.getFido2PrivilegedApiClient(GeckoAppShell.getApplicationContext());
      final Task<List<FidoCredentialDetails>> task = fidoClient.getCredentialList(rpId);
      task.addOnSuccessListener(
          credentials -> {
            for (final FidoCredentialDetails credential : credentials) {
              if (credential.getIsDiscoverable()) {
                // All discoverable credentials have to be handled by credential manager
                continue;
              }

              for (final WebAuthnUtils.WebAuthnPublicCredential item : allowList) {
                if (Arrays.equals(credential.getCredentialId(), item.id)) {
                  // Found credential in GMS.
                  result.complete(true);
                  return;
                }
              }
            }
            result.complete(false);
          });
      task.addOnFailureListener(e -> result.complete(false));
    } catch (final java.lang.Exception e) {
      // Play service may be nothing.
      return GeckoResult.fromValue(false);
    }
    return result;
  }
}
