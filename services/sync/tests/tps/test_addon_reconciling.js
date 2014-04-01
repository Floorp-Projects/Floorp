/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that record reconciling works as expected. It makes
// similar changes to add-ons in separate profiles and does a sync to verify
// the proper action is taken.
EnableEngines(["addons"]);

let phases = {
  "phase01": "profile1",
  "phase02": "profile2",
  "phase03": "profile1",
  "phase04": "profile2",
  "phase05": "profile1",
  "phase06": "profile2"
};

const id = "restartless-xpi@tests.mozilla.org";

// Install the add-on in 2 profiles.
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

// Now we disable in one and uninstall in the other.
Phase("phase03", [
  [Sync], // Get GUID updates, potentially.
  [Addons.setEnabled, [id], STATE_DISABLED],
]);
Phase("phase04", [
  [EnsureTracking],
  [Addons.uninstall, [id]],
  [Sync]
]);

// When we sync, the uninstall should take precedence because it was newer.
Phase("phase05", [
  [Sync]
]);
Phase("phase06", [
  [Sync],
  [Addons.verifyNot, [id]]
]);
