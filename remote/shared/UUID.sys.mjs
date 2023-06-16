/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Creates a unique UUID without enclosing curly brackets
 * Example: '86c832d2-cf1c-4001-b3e0-8628fdd41b29'
 *
 * @returns {string}
 *     The generated UUID as a string.
 */
export function generateUUID() {
  return Services.uuid.generateUUID().toString().slice(1, -1);
}
