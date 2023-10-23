/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This item shows image, title & subtitle.
// Once selected it will send fillMessageName with fillMessageData
// to the parent actor and response will be used to fill into the field.
export class GenericAutocompleteItem {
  comment = "";
  style = "generic";
  value = "";

  constructor(image, title, subtitle, fillMessageName, fillMessageData) {
    this.image = image;
    this.comment = JSON.stringify({
      title,
      subtitle,
      fillMessageName,
      fillMessageData,
    });
  }
}

/**
 * Show confirmation tooltip
 *
 * @param {object} browser - An object representing the browser.
 * @param {string} messageId - Message ID from browser/browser.properties.
 * @param {string} [anchorId="identity-icon"] - ID of the element to anchor the hint to.
 */
export function showConfirmation(
  browser,
  messageId,
  anchorId = "identity-icon"
) {
  const anchor = browser.ownerDocument.getElementById(anchorId);
  anchor.ownerGlobal.ConfirmationHint.show(anchor, messageId, {});
}
