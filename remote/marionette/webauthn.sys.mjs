/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "webauthnTransport",
  "@mozilla.org/webauthn/transport;1",
  "nsIWebAuthnTransport"
);

/** @namespace */
export const webauthn = {};

/**
 * Add a virtual authenticator.
 *
 * @param {string} protocol one of "ctap1/u2f", "ctap2", "ctap2_1"
 * @param {string} transport one of "usb", "nfc", "ble", "smart-card",
 *                 "hybrid", "internal"
 * @param {boolean} hasResidentKey
 * @param {boolean} hasUserVerification
 * @param {boolean} isUserConsenting
 * @param {boolean} isUserVerified
 * @returns {id} the id of the added authenticator
 */
webauthn.addVirtualAuthenticator = function (
  protocol,
  transport,
  hasResidentKey,
  hasUserVerification,
  isUserConsenting,
  isUserVerified
) {
  return lazy.webauthnTransport.addVirtualAuthenticator(
    protocol,
    transport,
    hasResidentKey,
    hasUserVerification,
    isUserConsenting,
    isUserVerified
  );
};

/**
 * Removes a virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 */
webauthn.removeVirtualAuthenticator = function (authenticatorId) {
  lazy.webauthnTransport.removeVirtualAuthenticator(authenticatorId);
};

/**
 * Adds a credential to a previously-added virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 * @param {string} credentialId a probabilistically-unique byte sequence
 *                 identifying a public key credential source and its
 *                 authentication assertions (encoded using Base64url
 *                 Encoding).
 * @param {boolean} isResidentCredential if set to true, a client-side
 *                  discoverable credential is created. If set to false, a
 *                  server-side credential is created instead.
 * @param {string} rpId The Relying Party ID the credential is scoped to.
 * @param {string} privateKey An asymmetric key package containing a single
 *                 private key per RFC5958, encoded using Base64url Encoding.
 * @param {string} userHandle The userHandle associated to the credential
 *                 encoded using Base64url Encoding.
 * @param {number} signCount The initial value for a signature counter
 *                 associated to the public key credential source.
 */
webauthn.addCredential = function (
  authenticatorId,
  credentialId,
  isResidentCredential,
  rpId,
  privateKey,
  userHandle,
  signCount
) {
  lazy.webauthnTransport.addCredential(
    authenticatorId,
    credentialId,
    isResidentCredential,
    rpId,
    privateKey,
    userHandle,
    signCount
  );
};

/**
 * Gets all credentials from a virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 * @returns {object} the credentials on the authenticator
 */
webauthn.getCredentials = function (authenticatorId) {
  return lazy.webauthnTransport.getCredentials(authenticatorId);
};

/**
 * Removes a credential from a virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 * @param {string} credentialId the id of the credential
 */
webauthn.removeCredential = function (authenticatorId, credentialId) {
  lazy.webauthnTransport.removeCredential(authenticatorId, credentialId);
};

/**
 * Removes all credentials from a virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 */
webauthn.removeAllCredentials = function (authenticatorId) {
  lazy.webauthnTransport.removeAllCredentials(authenticatorId);
};

/**
 * Sets the "isUserVerified" bit on a virtual authenticator.
 *
 * @param {id} authenticatorId the id of the virtual authenticator
 * @param {bool} isUserVerified the value to set the "isUserVerified" bit to
 */
webauthn.setUserVerified = function (authenticatorId, isUserVerified) {
  lazy.webauthnTransport.setUserVerified(authenticatorId, isUserVerified);
};
