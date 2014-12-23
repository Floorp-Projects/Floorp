/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AboutReader", "resource://gre/modules/AboutReader.jsm");

let dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "Content");

let AboutReaderListener = {
  init: function(chromeGlobal) {
    chromeGlobal.addEventListener("AboutReaderContentLoaded", this, false, true);
  },

  handleEvent: function(event) {
    if (!event.originalTarget.documentURI.startsWith("about:reader")) {
      return;
    }

    switch (event.type) {
      case "AboutReaderContentLoaded":
        // If we are restoring multiple reader mode tabs during session restore, duplicate "DOMContentLoaded"
        // events may be fired for the visible tab. The inital "DOMContentLoaded" may be received before the
        // document body is available, so we avoid instantiating an AboutReader object, expecting that a
        // valid message will follow. See bug 925983.
        if (content.document.body) {
          new AboutReader(content.document, content);
        }
        break;
    }
  }
};
AboutReaderListener.init(this);
