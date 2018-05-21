/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{

ChromeUtils.import("resource://gre/modules/Services.jsm");

class MozStringbundle extends MozXULElement {
  get stringBundle() {
    if (!this._bundle) {
      try {
        this._bundle = Services.strings.createBundle(this.src);
      } catch (e) {
        dump("Failed to get stringbundle:\n");
        dump(e + "\n");
      }
    }
    return this._bundle;
  }

  set src(val) {
    this._bundle = null;
    this.setAttribute("src", val);
    return val;
  }

  get src() {
    return this.getAttribute("src");
  }

  get strings() {
    // Note: this is a sucky method name! Should be:
    //       readonly attribute nsISimpleEnumerator strings;
    return this.stringBundle.getSimpleEnumeration();
  }

  getString(aStringKey) {
    try {
      return this.stringBundle.GetStringFromName(aStringKey);
    } catch (e) {
      dump("*** Failed to get string " + aStringKey + " in bundle: " + this.src + "\n");
      throw e;
    }
  }

  getFormattedString(aStringKey, aStringsArray) {
    try {
      return this.stringBundle.formatStringFromName(aStringKey, aStringsArray, aStringsArray.length);
    } catch (e) {
      dump("*** Failed to format string " + aStringKey + " in bundle: " + this.src + "\n");
      throw e;
    }
  }
}

customElements.define("stringbundle", MozStringbundle);

}
