/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

function ColorPicker() {
}

ColorPicker.prototype = {
  _initial: 0,
  _domWin: null,
  _title: "",

  get strings() {
    delete this.strings;
    return this.strings = Services.strings.createBundle("chrome://browser/locale/browser.properties");
  },

  init: function(aParent, aTitle, aInitial) {
    this._domWin = aParent;
    this._initial = aInitial;
    this._title = aTitle;
  },

  open: function(aCallback) {
    let p = new Prompt({ title: this._title,
                         buttons: [
                            this.strings.GetStringFromName("inputWidgetHelper.set"),
                            this.strings.GetStringFromName("inputWidgetHelper.cancel")
                         ] })
                      .addColorPicker({ value: this._initial })
                      .show((data) => {
      if (data.button == 0)
        aCallback.done(data.color0);
      else
        aCallback.done(this._initial);
    });
  },

  classID: Components.ID("{430b987f-bb9f-46a3-99a5-241749220b29}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIColorPicker])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ColorPicker]);
