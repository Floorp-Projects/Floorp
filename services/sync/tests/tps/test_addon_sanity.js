/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

EnableEngines(["addons"]);

let phases = { "phase1": "profile1",
               "phase2": "profile1" };

const id = "unsigned-xpi@tests.mozilla.org";

Phase("phase1", [
  [Addons.install, [id]],
  // Non-restartless add-on shouldn't be found after install.
  [Addons.verifyNot, [id]],

  // But it should be marked for Sync.
  [Sync]
]);

Phase("phase2", [
  // Add-on should be present after restart
  [Addons.verify, [id], STATE_ENABLED]
]);
