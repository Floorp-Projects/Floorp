/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This item shows image, label & secondary.
// Once selected it will send fillMessageName with fillMessageData
// to the parent actor and response will be used to fill into the field.
export class GenericAutocompleteItem {
  comment = "";
  style = "generic";
  value = "";

  constructor(image, label, secondary, fillMessageName, fillMessageData) {
    this.image = image;
    this.label = label;
    this.comment = JSON.stringify({
      secondary,
      fillMessageName,
      fillMessageData,
    });
  }
}

/**
 * Show confirmation tooltip
 *
 * @param {object} browser - An object representing the browser.
 * @param {string} messageId - Message ID from browser/confirmationHints.ftl
 * @param {string} [anchorId="identity-icon-box"] - ID of the element to anchor the hint to.
                   The "password-notification-icon" and "notification-popup-box" are hidden
                   at the point of showing the hint (for *most* cases), so approximate the
                   location with the next closest, visible icon as the anchor.
 */
export function showConfirmation(
  browser,
  messageId,
  anchorId = "identity-icon-box"
) {
  const anchor = browser.ownerDocument.getElementById(anchorId);
  anchor.ownerGlobal.ConfirmationHint.show(anchor, messageId, {});
}
