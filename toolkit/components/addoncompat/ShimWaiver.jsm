// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["ShimWaiver"];

this.ShimWaiver = {
  getProperty: function(obj, prop) {
    let rv = obj[prop];
    if (rv instanceof Function) {
      rv = rv.bind(obj);
    }
    return rv;
  }
};
