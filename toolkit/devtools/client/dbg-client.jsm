/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Just a compatibility wrapper for addons that are used
// to import the jsm instead of the js module

const Cu = Components.utils;

const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

this.EXPORTED_SYMBOLS = ["DebuggerTransport",
                         "DebuggerClient",
                         "RootClient",
                         "LongStringClient",
                         "EnvironmentClient",
                         "ObjectClient"];

var client = require("devtools/toolkit/client/main");

this.DebuggerClient = client.DebuggerClient;
this.RootClient = client.RootClient;
this.LongStringClient = client.LongStringClient;
this.EnvironmentClient = client.EnvironmentClient;
this.ObjectClient = client.ObjectClient;

this.DebuggerTransport = require("devtools/toolkit/transport/transport").DebuggerTransport;
