/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Integer } = asn1js.asn1js;
const { fromBase64, stringToArrayBuffer } = pvutils.pvutils;

export const b64urltodec = b64 => {
  return new Integer({
    valueHex: stringToArrayBuffer(fromBase64("AQAB", true, true)),
  }).valueBlock._valueDec;
};

export const b64urltohex = b64 => {
  const hexBuffer = new Integer({
    valueHex: stringToArrayBuffer(fromBase64(b64, true, true)),
  }).valueBlock._valueHex;
  const hexArray = Array.from(new Uint8Array(hexBuffer));

  return hexArray.map(b => ("00" + b.toString(16)).slice(-2));
};

// this particular prototype override makes it easy to chain down complex objects
export const getObjPath = (obj, path) => {
  path = path.split(".");
  for (let i = 0, len = path.length; i < len; i++) {
    if (Array.isArray(obj[path[i]])) {
      obj = obj[path[i]][path[i + 1]];
      i++;
    } else {
      obj = obj[path[i]];
    }
  }
  return obj;
};

export const hash = async (algo, buffer) => {
  const hashBuffer = await crypto.subtle.digest(algo, buffer);
  const hashArray = Array.from(new Uint8Array(hashBuffer));

  return hashArray
    .map(b => ("00" + b.toString(16)).slice(-2))
    .join(":")
    .toUpperCase();
};

export const hashify = hash => {
  if (typeof hash === "string") {
    return hash
      .match(/.{2}/g)
      .join(":")
      .toUpperCase();
  }
  return hash.join(":").toUpperCase();
};

export const pemToDER = pem => {
  return stringToArrayBuffer(window.atob(pem));
};

export const normalizeToKebabCase = string => {
  let kebabString = string
    .replace(/\s+/g, "-")
    .replace(/\./g, "")
    .replace(/\//g, "")
    .replace(/--/g, "-")
    .toLowerCase();

  return kebabString;
};
