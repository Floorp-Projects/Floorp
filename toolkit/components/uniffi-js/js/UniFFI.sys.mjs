/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS module contains shared functionality for the generated UniFFI JS
// code.

// TypeError for UniFFI calls
//
// This extends TypeError to add support for recording a nice description of
// the item that fails the type check. This is especially useful for invalid
// values nested in objects/arrays/maps, etc.
//
// To accomplish this, the FfiConverter.checkType methods of records, arrays,
// maps, etc. catch UniFFITypeError, call `addItemDescriptionPart()` with a
// string representing the child item, then re-raise the exception.  We then
// join all the parts together, in reverse order, to create item description
// strings like `foo.bar[123]["key"]`
export class UniFFITypeError extends TypeError {
  constructor(reason) {
    super();
    this.reason = reason;
    this.itemDescriptionParts = [];
  }

  addItemDescriptionPart(part) {
    this.itemDescriptionParts.push(part);
  }

  itemDescription() {
    const itemDescriptionParts = [...this.itemDescriptionParts];
    itemDescriptionParts.reverse();
    return itemDescriptionParts.join("");
  }

  get message() {
    return `${this.itemDescription()}: ${this.reason}`;
  }
}
