// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;

const DEBUGGER_REMOTE_ENABLED = "devtools.debugger.remote-enabled";

Cu.import("resource://gre/modules/Services.jsm");

add_test(function() {
  let window = Services.wm.getMostRecentWindow("navigator:browser");

  // Enable the debugger via the pref it listens for
  do_register_cleanup(function() {
    Services.prefs.clearUserPref(DEBUGGER_REMOTE_ENABLED);
  });
  Services.prefs.setBoolPref(DEBUGGER_REMOTE_ENABLED, true);

  let DebuggerServer = window.DebuggerServer;
  do_check_true(DebuggerServer.initialized);
  do_check_true(!!DebuggerServer._listener);

  run_next_test();
});

run_next_test();
