/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Loads the remote debugging protocol code into a sandbox, in order to
 * shield it from the debuggee. This way, when debugging chrome globals,
 * debugger and debuggee will be in separate compartments.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

var EXPORTED_SYMBOLS = ["DebuggerServer"];

function loadSubScript(aURL)
{
  try {
    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
      .getService(Ci.mozIJSSubScriptLoader);
    loader.loadSubScript(aURL, this);
  } catch(e) {
    dump("Error loading: " + aURL + ": " + e + " - " + e.stack + "\n");

    throw e;
  }
}

Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

// Load the debugging server in a sandbox with its own compartment.
var systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
                      .createInstance(Ci.nsIPrincipal);

var gGlobal = Cu.Sandbox(systemPrincipal);
gGlobal.importFunction(loadSubScript);
gGlobal.loadSubScript("chrome://global/content/devtools/dbg-server.js");

var DebuggerServer = gGlobal.DebuggerServer;
