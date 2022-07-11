/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Locked status should be communicated to children.

function isParentProcess() {
  return Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  const pb = Services.prefs;

  let bprefname = "Test.IPC.locked.bool";
  let iprefname = "Test.IPC.locked.int";
  let sprefname = "Test.IPC.locked.string";

  let isParent = isParentProcess();
  if (isParent) {
    pb.setBoolPref(bprefname, true);
    pb.lockPref(bprefname);

    pb.setIntPref(iprefname, true);
    pb.lockPref(iprefname);

    pb.setStringPref(sprefname, true);
    pb.lockPref(sprefname);
    pb.unlockPref(sprefname);

    run_test_in_child("test_locked_prefs.js");
  }

  ok(pb.prefIsLocked(bprefname), bprefname + " should be locked in the child");
  ok(pb.prefIsLocked(iprefname), iprefname + " should be locked in the child");
  ok(
    !pb.prefIsLocked(sprefname),
    sprefname + " should be unlocked in the child"
  );
}
