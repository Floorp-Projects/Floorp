/* -*- Mode: Java; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=100: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.Base64;
import com.google.android.gms.fido.common.Transport;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import org.mozilla.gecko.annotation.WrapForJNI;

public class WebAuthnUtils {
  private static final String LOGTAG = "WebAuthnUtils";
  private static final boolean DEBUG = false;

  // from dom/webauthn/WebAuthnTransportIdentifiers.h
  private static final byte AUTHENTICATOR_TRANSPORT_USB = 1;
  private static final byte AUTHENTICATOR_TRANSPORT_NFC = 2;
  private static final byte AUTHENTICATOR_TRANSPORT_BLE = 4;
  private static final byte AUTHENTICATOR_TRANSPORT_INTERNAL = 8;

  // From WebAuthentication.webidl
  public enum AttestationPreference {
    NONE,
    INDIRECT,
    DIRECT,
  }

  public static List<Transport> getTransportsForByte(final byte transports) {
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
    if ((transports & AUTHENTICATOR_TRANSPORT_INTERNAL) == AUTHENTICATOR_TRANSPORT_INTERNAL) {
      result.add(Transport.INTERNAL);
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

    public static ArrayList<WebAuthnPublicCredential> CombineBuffers(
        final Object[] idObjectList, final ByteBuffer transportList) {
      if (idObjectList.length != transportList.remaining()) {
        throw new RuntimeException("Couldn't extract allowed list!");
      }

      final ArrayList<WebAuthnPublicCredential> credList =
          new ArrayList<WebAuthnPublicCredential>();

      final byte[] transportBytes = new byte[transportList.remaining()];
      transportList.get(transportBytes);

      for (int i = 0; i < idObjectList.length; i++) {
        final ByteBuffer id = (ByteBuffer) idObjectList[i];
        final byte[] idBytes = new byte[id.remaining()];
        id.get(idBytes);

        credList.add(new WebAuthnPublicCredential(idBytes, transportBytes[i]));
      }
      return credList;
    }
  }

  @WrapForJNI
  public static class MakeCredentialResponse {
    public final byte[] clientDataJson;
    public final byte[] keyHandle;
    public final byte[] attestationObject;
    public final String[] transports;
    public final String authenticatorAttachment;

    public static final class Builder {
      private byte[] mClientDataJson;
      private byte[] mKeyHandle;
      private byte[] mAttestationObject;
      private String[] mTransports;
      private String mAuthenticatorAttachment;

      public Builder() {}

      public Builder setClientDataJson(final byte[] clientDataJson) {
        this.mClientDataJson = clientDataJson;
        return this;
      }

      public Builder setKeyHandle(final byte[] keyHandle) {
        this.mKeyHandle = keyHandle;
        return this;
      }

      public Builder setAttestationObject(final byte[] attestationObject) {
        this.mAttestationObject = attestationObject;
        return this;
      }

      public Builder setTransports(final String[] transports) {
        this.mTransports = transports;
        return this;
      }

      public Builder setAuthenticatorAttachment(final String authenticatorAttachment) {
        this.mAuthenticatorAttachment = authenticatorAttachment;
        return this;
      }

      public MakeCredentialResponse build() {
        return new MakeCredentialResponse(this);
      }
    }

    @WrapForJNI(skip = true)
    protected MakeCredentialResponse(final Builder builder) {
      this.clientDataJson = builder.mClientDataJson;
      this.keyHandle = builder.mKeyHandle;
      this.attestationObject = builder.mAttestationObject;
      this.transports = builder.mTransports;
      this.authenticatorAttachment = builder.mAuthenticatorAttachment;
    }

    @WrapForJNI(skip = true)
    @Override
    public String toString() {
      final StringBuilder sb = new StringBuilder("{");
      sb.append("clientDataJson=")
          .append(
              Base64.encodeToString(
                  this.clientDataJson, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", keyHandle=")
          .append(
              Base64.encodeToString(
                  this.keyHandle, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", attestationObject=")
          .append(
              Base64.encodeToString(
                  this.attestationObject, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", transports=")
          .append(String.join(", ", this.transports))
          .append(", authenticatorAttachment=")
          .append(this.authenticatorAttachment)
          .append("}");
      return sb.toString();
    }
  }

  @WrapForJNI
  public static class GetAssertionResponse {
    public final byte[] clientDataJson;
    public final byte[] keyHandle;
    public final byte[] authData;
    public final byte[] signature;
    public final byte[] userHandle;
    public final String authenticatorAttachment;

    public static final class Builder {
      private byte[] mClientDataJson;
      private byte[] mKeyHandle;
      private byte[] mAuthData;
      private byte[] mSignature;
      private byte[] mUserHandle;
      private String mAuthenticatorAttachment;

      public Builder() {}

      public Builder setClientDataJson(final byte[] clientDataJson) {
        this.mClientDataJson = clientDataJson;
        return this;
      }

      public Builder setKeyHandle(final byte[] keyHandle) {
        this.mKeyHandle = keyHandle;
        return this;
      }

      public Builder setAuthData(final byte[] authData) {
        this.mAuthData = authData;
        return this;
      }

      public Builder setSignature(final byte[] signature) {
        this.mSignature = signature;
        return this;
      }

      public Builder setUserHandle(final byte[] userHandle) {
        this.mUserHandle = userHandle;
        return this;
      }

      public Builder setAuthenticatorAttachment(final String authenticatorAttachment) {
        this.mAuthenticatorAttachment = authenticatorAttachment;
        return this;
      }

      public GetAssertionResponse build() {
        return new GetAssertionResponse(this);
      }
    }

    @WrapForJNI(skip = true)
    protected GetAssertionResponse(final Builder builder) {
      this.clientDataJson = builder.mClientDataJson;
      this.keyHandle = builder.mKeyHandle;
      this.authData = builder.mAuthData;
      this.signature = builder.mSignature;
      if (builder.mUserHandle == null) {
        this.userHandle = new byte[0];
      } else {
        this.userHandle = builder.mUserHandle;
      }
      this.authenticatorAttachment = builder.mAuthenticatorAttachment;
    }

    @WrapForJNI(skip = true)
    @Override
    public String toString() {
      final StringBuilder sb = new StringBuilder("{");
      sb.append("clientDataJson=")
          .append(
              Base64.encodeToString(
                  this.clientDataJson, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", keyHandle=")
          .append(
              Base64.encodeToString(
                  this.keyHandle, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", authData=")
          .append(
              Base64.encodeToString(
                  this.authData, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", signature=")
          .append(
              Base64.encodeToString(
                  this.signature, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING))
          .append(", userHandle=")
          .append(
              userHandle.length > 0
                  ? Base64.encodeToString(
                      this.userHandle, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING)
                  : "(empty)")
          .append(", authenticatorAttachment=")
          .append(this.authenticatorAttachment)
          .append("}");
      return sb.toString();
    }
  }
}
