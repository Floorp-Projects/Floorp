/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of "requestAutocomplete.xhtml".
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const RequestAutocompleteDialog = {
  resolveFn: null,

  onLoad: function () {
    Task.spawn(function* () {
      this.resolveFn = window.arguments[0].wrappedJSObject.resolveFn;

      window.sizeToContent();

      Services.obs.notifyObservers(window,
                                   "formautofill-window-initialized", "");
    }.bind(this)).catch(Cu.reportError);
  },

  onAccept: function () {
    window.close();
    this.resolveFn({ email: "email@example.org" });
  },

  onCancel: function () {
    window.close();
    this.resolveFn({ canceled: true });
  },
};
