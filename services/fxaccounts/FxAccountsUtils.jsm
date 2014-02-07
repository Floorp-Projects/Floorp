/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FxAccountsUtils"];

this.FxAccountsUtils = Object.freeze({
  /**
   * Copies properties from a given object to another object.
   *
   * @param from (object)
   *        The object we read property descriptors from.
   * @param to (object)
   *        The object that we set property descriptors on.
   * @param options (object) (optional)
   *        {keys: [...]}
   *          Lets the caller pass the names of all properties they want to be
   *          copied. Will copy all properties of the given source object by
   *          default.
   *        {bind: object}
   *          Lets the caller specify the object that will be used to .bind()
   *          all function properties we find to. Will bind to the given target
   *          object by default.
   */
  copyObjectProperties: function (from, to, opts = {}) {
    let keys = (opts && opts.keys) || Object.keys(from);
    let thisArg = (opts && opts.bind) || to;

    for (let prop of keys) {
      let desc = Object.getOwnPropertyDescriptor(from, prop);

      if (typeof(desc.value) == "function") {
        desc.value = desc.value.bind(thisArg);
      }

      if (desc.get) {
        desc.get = desc.get.bind(thisArg);
      }

      if (desc.set) {
        desc.set = desc.set.bind(thisArg);
      }

      Object.defineProperty(to, prop, desc);
    }
  }
});
