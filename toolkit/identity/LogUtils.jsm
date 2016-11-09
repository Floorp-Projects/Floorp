/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Logger"];
const PREF_DEBUG = "toolkit.identity.debug";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function IdentityLogger() {
  Services.prefs.addObserver(PREF_DEBUG, this, false);
  this._debug = Services.prefs.getBoolPref(PREF_DEBUG);
  return this;
}

IdentityLogger.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        this._debug = Services.prefs.getBoolPref(PREF_DEBUG);
        break;

      case "quit-application-granted":
        Services.prefs.removeObserver(PREF_DEBUG, this);
        break;

      default:
        this.log("Logger observer", "Unknown topic:", aTopic);
        break;
    }
  },

  _generateLogMessage: function _generateLogMessage(aPrefix, args) {
    // create a string representation of a list of arbitrary things
    let strings = [];

    // XXX bug 770418 - args look like flattened array, not list of strings

    args.forEach(function(arg) {
      if (typeof arg === 'string') {
        strings.push(arg);
      } else if (typeof arg === 'undefined') {
        strings.push('undefined');
      } else if (arg === null) {
        strings.push('null');
      } else {
        try {
          strings.push(JSON.stringify(arg, null, 2));
        } catch (err) {
          strings.push("<<something>>");
        }
      }
    });
    return 'Identity ' + aPrefix + ': ' + strings.join(' ');
  },

  /**
   * log() - utility function to print a list of arbitrary things
   *
   * Enable with about:config pref toolkit.identity.debug
   */
  log: function log(aPrefix, ...args) {
    if (!this._debug) {
      return;
    }
    let output = this._generateLogMessage(aPrefix, args);
    dump(output + "\n");

    // Additionally, make the output visible in the Error Console
    Services.console.logStringMessage(output);
  },

  /**
   * reportError() - report an error through component utils as well as
   * our log function
   */
  reportError: function reportError(aPrefix, ...aArgs) {
    // Report the error in the browser
    let output = this._generateLogMessage(aPrefix, aArgs);
    Cu.reportError(output);
    dump("ERROR: " + output + "\n");
    for (let frame = Components.stack.caller; frame; frame = frame.caller) {
      dump(frame + "\n");
    }
  }

};

this.Logger = new IdentityLogger();
