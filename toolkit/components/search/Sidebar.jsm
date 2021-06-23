/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function nsSidebar() {}

nsSidebar.prototype = {
  init(window) {
    this.window = window;
  },

  // This function implements window.external.AddSearchProvider(),
  // for compatibility with older browsers.  The function has been deprecated
  // and so will not be implemented.
  AddSearchProvider(engineURL) {},

  // This function exists to implement window.external.IsSearchProviderInstalled(),
  // for compatibility with other browsers.  The function has been deprecated
  // and so will not be implemented.
  IsSearchProviderInstalled() {},

  classID: Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}"),
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
};

var EXPORTED_SYMBOLS = ["nsSidebar"];
