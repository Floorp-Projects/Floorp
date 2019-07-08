// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineModuleGetter(
  this,
  "Snackbars",
  "resource://gre/modules/Snackbars.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "EventDispatcher",
  "resource://gre/modules/Messaging.jsm"
);

add_task(async function test_snackbar_api() {
  Snackbars.show("This is a Snackbar", Snackbars.LENGTH_INDEFINITE, {
    action: {
      label: "Click me",
      callback: function() {},
    },
  });

  await EventDispatcher.instance.sendRequestForResult({
    type: "Robocop:WaitOnUI",
  });
});

run_next_test();
