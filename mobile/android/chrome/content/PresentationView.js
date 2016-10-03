/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

// globals Services
Cu.import("resource://gre/modules/Services.jsm");

function log(str) {
  // dump("-*- PresentationView.js -*-: " + str + "\n");
}

let PresentationView = {
  _id: null,

  startup: function startup() {
    // use hash as the ID of this top level window
    this._id = window.location.hash.substr(1);
  },
};
