// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Accounts.jsm");

add_task(function test_Accounts() {
  let syncExists = yield Accounts.syncAccountsExist();
  dump("Sync account exists? " + syncExists + "\n");
  let firefoxExists = yield Accounts.firefoxAccountsExist();
  dump("Firefox account exists? " + firefoxExists + "\n");
  let anyExists = yield Accounts.anySyncAccountsExist();
  dump("Any accounts exist? " + anyExists + "\n");

  // Only one account should exist.
  do_check_true(!syncExists || !firefoxExists);
  do_check_eq(anyExists, firefoxExists || syncExists);

  dump("Launching setup.\n");
  Accounts.launchSetup();
});

run_next_test();
