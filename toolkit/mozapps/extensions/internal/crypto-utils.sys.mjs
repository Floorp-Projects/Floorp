/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CryptoHash = Components.Constructor(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);

export function computeHashAsString(hashType, input) {
  const data = new Uint8Array(new TextEncoder().encode(input));
  const crypto = CryptoHash(hashType);
  crypto.update(data, data.length);
  return getHashStringForCrypto(crypto);
}

/**
 * Returns the string representation (hex) of the SHA256 hash of `input`.
 *
 * @param {string} input
 *        The value to hash.
 * @returns {string}
 *          The hex representation of a SHA256 hash.
 */
export function computeSha256HashAsString(input) {
  return computeHashAsString("sha256", input);
}

/**
 * Returns the string representation (hex) of the SHA1 hash of `input`.
 *
 * @param {string} input
 *        The value to hash.
 * @returns {string}
 *          The hex representation of a SHA1 hash.
 */
export function computeSha1HashAsString(input) {
  return computeHashAsString("sha1", input);
}

/**
 * Returns the string representation (hex) of a given CryptoHashInstance.
 *
 * @param {CryptoHash} aCrypto
 * @returns {string}
 *          The hex representation of a SHA256 hash.
 */
export function getHashStringForCrypto(aCrypto) {
  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  // convert the binary hash data to a hex string.
  let binary = aCrypto.finish(/* base64 */ false);
  let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)));
  return hash.join("").toLowerCase();
}
