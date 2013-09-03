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

this.EXPORTED_SYMBOLS = ["DebuggerServer", "ActorPool"];

var loadSubScript =
  "function loadSubScript(aURL)\n" +
  "{\n" +
  "const Ci = Components.interfaces;\n" +
  "const Cc = Components.classes;\n" +
  "  try {\n" +
  "    let loader = Cc[\"@mozilla.org/moz/jssubscript-loader;1\"]\n" +
  "      .getService(Ci.mozIJSSubScriptLoader);\n" +
  "    loader.loadSubScript(aURL, this);\n" +
  "  } catch(e) {\n" +
  "    dump(\"Error loading: \" + aURL + \": \" + e + \" - \" + e.stack + \"\\n\");\n" +
  "    throw e;\n" +
  "  }\n" +
  "}";

// Load the debugging server in a sandbox with its own compartment.
var systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
                      .createInstance(Ci.nsIPrincipal);

var gGlobal = Cu.Sandbox(systemPrincipal);
Cu.evalInSandbox(loadSubScript, gGlobal, "1.8");
gGlobal.loadSubScript("resource://gre/modules/devtools/server/main.js");

this.DebuggerServer = gGlobal.DebuggerServer;
this.ActorPool = gGlobal.ActorPool;
