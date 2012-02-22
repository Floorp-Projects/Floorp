/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that install of restartless extensions syncs to
// other profiles.
EnableEngines(["addons"]);

let phases = {
  "phase01": "profile1",
  "phase02": "profile2",
  "phase03": "profile1",
  "phase04": "profile2",
  "phase05": "profile1",
  "phase06": "profile2",
  "phase07": "profile1",
  "phase08": "profile2"
};

const id = "restartless-xpi@tests.mozilla.org";

// Verify install is synced
Phase("phase01", [
  [Addons.verifyNot, [id]],
  [Addons.install, [id]],
  [Addons.verify, [id], STATE_ENABLED],
  [Sync]
]);
Phase("phase02", [
  [Addons.verifyNot, [id]],
  [Sync],
  [Addons.verify, [id], STATE_ENABLED]
]);

// Now disable and see that is is synced.
Phase("phase03", [
  [EnsureTracking],
  [Addons.setEnabled, [id], STATE_DISABLED],
  [Addons.verify, [id], STATE_DISABLED],
  [Sync]
]);
Phase("phase04", [
  [Sync],
  [Addons.verify, [id], STATE_DISABLED]
]);

// Enable and see it is synced.
Phase("phase05", [
  [EnsureTracking],
  [Addons.setEnabled, [id], STATE_ENABLED],
  [Addons.verify, [id], STATE_ENABLED],
  [Sync]
]);
Phase("phase06", [
  [Sync],
  [Addons.verify, [id], STATE_ENABLED]
]);

// Uninstall and see it is synced.
Phase("phase07", [
  [EnsureTracking],
  [Addons.verify, [id], STATE_ENABLED],
  [Addons.uninstall, [id]],
  [Addons.verifyNot, [id]],
  [Sync]
]);
Phase("phase08", [
  [Addons.verify, [id], STATE_ENABLED],
  [Sync],
  [Addons.verifyNot, [id]]
]);
