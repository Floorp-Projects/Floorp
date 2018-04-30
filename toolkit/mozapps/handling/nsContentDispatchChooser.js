/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Constants

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const CONTENT_HANDLING_URL = "chrome://mozapps/content/handling/dialog.xul";
const STRINGBUNDLE_URL = "chrome://mozapps/locale/handling/handling.properties";

// nsContentDispatchChooser class

function nsContentDispatchChooser() {
}

nsContentDispatchChooser.prototype =
{
  classID: Components.ID("e35d5067-95bc-4029-8432-e8f1e431148d"),

  // nsIContentDispatchChooser

  ask: function ask(aHandler, aWindowContext, aURI, aReason) {
    var window = null;
    try {
      if (aWindowContext)
        window = aWindowContext.getInterface(Ci.nsIDOMWindow);
    } catch (e) { /* it's OK to not have a window */ }

    var bundle = Services.strings.createBundle(STRINGBUNDLE_URL);

    // TODO when this is hooked up for content, we will need different strings
    //      for most of these
    var arr = [bundle.GetStringFromName("protocol.title"),
               "",
               bundle.GetStringFromName("protocol.description"),
               bundle.GetStringFromName("protocol.choices.label"),
               bundle.formatStringFromName("protocol.checkbox.label",
                                           [aURI.scheme], 1),
               bundle.GetStringFromName("protocol.checkbox.accesskey"),
               bundle.formatStringFromName("protocol.checkbox.extra",
                                           [Services.appinfo.name], 1)];

    var params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let SupportsString = Components.Constructor(
                           "@mozilla.org/supports-string;1",
                           "nsISupportsString");
    for (let text of arr) {
      let string = new SupportsString;
      string.data = text;
      params.appendElement(string);
    }
    params.appendElement(aHandler);
    params.appendElement(aURI);
    params.appendElement(aWindowContext);

    Services.ww.openWindow(window,
                           CONTENT_HANDLING_URL,
                           null,
                           "chrome,dialog=yes,resizable,centerscreen",
                           params);
  },

  // nsISupports

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentDispatchChooser])
};

// Module

var components = [nsContentDispatchChooser];

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
