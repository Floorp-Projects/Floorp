/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function ContentDispatchChooser() {}

ContentDispatchChooser.prototype =
{
  classID: Components.ID("5a072a22-1e66-4100-afc1-07aed8b62fc5"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentDispatchChooser]),

  ask: function ask(aHandler, aWindowContext, aURI, aReason) {
    let window = null;
    try {
      if (aWindowContext)
        window = aWindowContext.getInterface(Ci.nsIDOMWindow);
    } catch (e) { /* it's OK to not have a window */ }

    let bundle = Services.strings.createBundle("chrome://mozapps/locale/handling/handling.properties");

    let title = bundle.GetStringFromName("protocol.title");
    let message = bundle.GetStringFromName("protocol.description");

    let open = Services.prompt.confirm(window, title, message);
    if (open)
      aHandler.launchWithURI(aURI, aWindowContext);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentDispatchChooser]);

