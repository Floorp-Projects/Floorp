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

let fillRequestId = 0;

/**
 * Send a message encoded in the comment from an autocomplete item
 * to the parent.
 *
 * @param {string} actorName name of the actor to send to
 * @param {object} autocompleteInput current nsIAutoCompleteInput
 * @param {string} comment serialized JSON comment containing fillMessageName and
 *                         fillMessageData to send to the actor
 */
export async function sendFillRequestToParent(
  actorName,
  autocompleteInput,
  comment
) {
  if (!comment) {
    return;
  }

  const { fillMessageName, fillMessageData } = JSON.parse(comment);
  if (!fillMessageName) {
    return;
  }

  fillRequestId++;
  const currentFillRequestId = fillRequestId;
  const actor =
    autocompleteInput.focusedInput.ownerGlobal?.windowGlobalChild?.getActor(
      actorName
    );
  const value = await actor.sendQuery(fillMessageName, fillMessageData ?? {});

  // skip fill if another fill operation started during await
  if (currentFillRequestId != fillRequestId) {
    return;
  }

  if (typeof value !== "string") {
    return;
  }

  // If the parent returned a string to fill, we must do it here because
  // nsAutoCompleteController.cpp already finished it's work before we finished await.
  autocompleteInput.textValue = value;
  autocompleteInput.selectTextRange(value.length, value.length);
}
