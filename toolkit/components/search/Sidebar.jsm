/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function nsSidebar() {}

nsSidebar.prototype = {
  init(window) {
    this.window = window;
    this.mm = window.docShell.messageManager;
  },

  // This function implements window.external.AddSearchProvider().
  // The capitalization, although nonstandard here, is to match other browsers'
  // APIs and is therefore important.
  AddSearchProvider(engineURL) {
    if (!Services.prefs.getBoolPref("dom.sidebar.enabled", false)) {
      return;
    }

    if (!this.mm) {
      Cu.reportError(
        `Installing a search provider from this context is not currently supported: ${
          Error().stack
        }.`
      );
      return;
    }

    this.mm.sendAsyncMessage("Search:AddEngine", {
      pageURL: this.window.document.documentURIObject.spec,
      engineURL,
    });
  },

  // This function exists to implement window.external.IsSearchProviderInstalled(),
  // for compatibility with other browsers.  The function has been deprecated
  // and so will not be implemented.
  IsSearchProviderInstalled() {},

  classID: Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};

var EXPORTED_SYMBOLS = ["nsSidebar"];
