/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2" };

/*
 * Test phases
 */

Phase('phase1', [
  [RunMozmillTest, 'mozmill_sanity.js'],
  [Sync, SYNC_WIPE_SERVER]
]);

Phase('phase2', [
  [Sync],
  [RunMozmillTest, 'mozmill_sanity2.js'],
]);
