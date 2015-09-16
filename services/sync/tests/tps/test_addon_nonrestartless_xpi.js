
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that install of extensions that require restart
// syncs between profiles.
EnableEngines(["addons"]);

var phases = {
  "phase01": "profile1",
  "phase02": "profile1",
  "phase03": "profile2",
  "phase04": "profile2",
  "phase05": "profile1",
  "phase06": "profile1",
  "phase07": "profile2",
  "phase08": "profile2",
  "phase09": "profile1",
  "phase10": "profile1",
  "phase11": "profile2",
  "phase12": "profile2",
  "phase13": "profile1",
  "phase14": "profile1",
  "phase15": "profile2",
  "phase16": "profile2"
};

const id = "unsigned-xpi@tests.mozilla.org";

Phase("phase01", [
  [Addons.verifyNot, [id]],
  [Addons.install, [id]],
  [Sync]
]);
Phase("phase02", [
  [Addons.verify, [id], STATE_ENABLED]
]);
Phase("phase03", [
  [Addons.verifyNot, [id]],
  [Sync]
]);
Phase("phase04", [
  [Addons.verify, [id], STATE_ENABLED],
]);

// Now we disable the add-on
Phase("phase05", [
  [EnsureTracking],
  [Addons.setEnabled, [id], STATE_DISABLED],
  [Sync]
]);
Phase("phase06", [
  [Addons.verify, [id], STATE_DISABLED],
]);
Phase("phase07", [
  [Addons.verify, [id], STATE_ENABLED],
  [Sync]
]);
Phase("phase08", [
  [Addons.verify, [id], STATE_DISABLED]
]);

// Now we re-enable it again.
Phase("phase09", [
  [EnsureTracking],
  [Addons.setEnabled, [id], STATE_ENABLED],
  [Sync]
]);
Phase("phase10", [
  [Addons.verify, [id], STATE_ENABLED],
]);
Phase("phase11", [
  [Addons.verify, [id], STATE_DISABLED],
  [Sync]
]);
Phase("phase12", [
  [Addons.verify, [id], STATE_ENABLED]
]);

// And we uninstall it

Phase("phase13", [
  [EnsureTracking],
  [Addons.verify, [id], STATE_ENABLED],
  [Addons.uninstall, [id]],
  [Sync]
]);
Phase("phase14", [
  [Addons.verifyNot, [id]]
]);
Phase("phase15", [
  [Addons.verify, [id], STATE_ENABLED],
  [Sync]
]);
Phase("phase16", [
  [Addons.verifyNot, [id]]
]);
