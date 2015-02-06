/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// File extension for Sherlock search plugin description files
const SHERLOCK_FILE_EXT_REGEXP = /\.src$/i;

function nsSidebar() {
}

nsSidebar.prototype = {
  init: function(window) {
    this.window = window;
    this.mm = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDocShell)
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIContentFrameMessageManager);
  },

  // The suggestedTitle and suggestedCategory parameters are ignored, but remain
  // for backward compatibility.
  addSearchEngine: function(engineURL, iconURL, suggestedTitle, suggestedCategory) {
    let dataType = SHERLOCK_FILE_EXT_REGEXP.test(engineURL) ?
                   Ci.nsISearchEngine.DATA_TEXT :
                   Ci.nsISearchEngine.DATA_XML;

    this.mm.sendAsyncMessage("Search:AddEngine", {
      pageURL: this.window.document.documentURIObject.spec,
      engineURL,
      type: dataType,
      iconURL
    });
  },

  // This function exists largely to implement window.external.AddSearchProvider(),
  // to match other browsers' APIs.  The capitalization, although nonstandard here,
  // is therefore important.
  AddSearchProvider: function(engineURL) {
    this.mm.sendAsyncMessage("Search:AddEngine", {
      pageURL: this.window.document.documentURIObject.spec,
      engineURL,
      type: Ci.nsISearchEngine.DATA_XML
    });
  },

  // This function exists to implement window.external.IsSearchProviderInstalled(),
  // for compatibility with other browsers.  The function has been deprecated
  // and so will not be implemented.
  IsSearchProviderInstalled: function(engineURL) {
    return 0;
  },

  classID: Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsSidebar]);
