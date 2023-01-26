/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const normalizeToKebabCase = string => {
  let kebabString = string
    // Turn all dots into dashes
    .replace(/\./g, "-")
    // Turn whitespace into dashes
    .replace(/\s+/g, "-")
    // Remove all non-characters or numbers
    .replace(/[^a-z0-9\-]/gi, "")
    // De-dupe dashes
    .replace(/--/g, "-")
    // Remove trailing and leading dashes
    .replace(/^-/g, "")
    .replace(/-$/g, "")
    .toLowerCase();

  return kebabString;
};

export const b64ToPEM = string => {
  let wrapped = string.match(/.{1,64}/g).join("\r\n");
  return `-----BEGIN CERTIFICATE-----\r\n${wrapped}\r\n-----END CERTIFICATE-----\r\n`;
};
