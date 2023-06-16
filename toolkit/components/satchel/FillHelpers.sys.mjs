/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This item shows icon, title & subtitle.
// Once selected it will send fillMessageName with fillMessageData
// to the parent actor and response will be used to fill into the field.
export class GenericAutocompleteItem {
  comment = "";
  style = "generic";
  value = "";

  constructor(icon, title, subtitle, fillMessageName, fillMessageData) {
    this.comment = JSON.stringify({
      icon,
      title,
      subtitle,
      fillMessageName,
      fillMessageData,
    });
  }
}
