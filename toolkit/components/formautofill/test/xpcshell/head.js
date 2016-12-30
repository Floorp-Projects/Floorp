/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Initialization specific to Form Autofill xpcshell tests.
 *
 * This file is loaded alongside loader.js.
 */

/* import-globals-from loader.js */

"use strict";

// The testing framework is fully initialized at this point, you can add
// xpcshell specific test initialization here.  If you need shared functions or
// initialization that are not specific to xpcshell, consider adding them to
// "head_common.js" in the parent folder instead.

add_task_in_parent_process(function* test_xpcshell_initialize_profile() {
  // We need to send the profile-after-change notification manually to the
  // startup component to ensure it has been initialized.
  Cc["@mozilla.org/formautofill/startup;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", "");
});
