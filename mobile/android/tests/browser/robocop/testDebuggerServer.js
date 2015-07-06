// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;

const DEBUGGER_USB_ENABLED = "devtools.remote.usb.enabled";

Cu.import("resource://gre/modules/Services.jsm");
const { DebuggerServer } =
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

add_test(function() {
  let window = Services.wm.getMostRecentWindow("navigator:browser");

  window.RemoteDebugger.init();

  // Enable the debugger via the pref it listens for
  do_register_cleanup(function() {
    Services.prefs.clearUserPref(DEBUGGER_USB_ENABLED);
  });
  Services.prefs.setBoolPref(DEBUGGER_USB_ENABLED, true);

  do_check_true(DebuggerServer.initialized);
  do_check_eq(DebuggerServer.listeningSockets, 1);

  run_next_test();
});

run_next_test();
