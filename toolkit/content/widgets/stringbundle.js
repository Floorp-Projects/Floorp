/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
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
        dump(
          "*** Failed to get string " +
            aStringKey +
            " in bundle: " +
            this.src +
            "\n"
        );
        throw e;
      }
    }

    getFormattedString(aStringKey, aStringsArray) {
      try {
        return this.stringBundle.formatStringFromName(
          aStringKey,
          aStringsArray
        );
      } catch (e) {
        dump(
          "*** Failed to format string " +
            aStringKey +
            " in bundle: " +
            this.src +
            "\n"
        );
        throw e;
      }
    }
  }

  customElements.define("stringbundle", MozStringbundle);
}
