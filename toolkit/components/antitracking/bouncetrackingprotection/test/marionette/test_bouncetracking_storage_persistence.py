# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase

# Tests the persistence of the bounce tracking protection storage across
# restarts.


class BounceTrackingStoragePersistenceTestCase(MarionetteTestCase):
    def setUp(self):
        super(BounceTrackingStoragePersistenceTestCase, self).setUp()
        self.marionette.enforce_gecko_prefs(
            {
                "privacy.bounceTrackingProtection.enabled": True,
                "privacy.bounceTrackingProtection.enableTestMode": True,
            }
        )

        self.marionette.set_context("chrome")
        self.populate_state()

    def populate_state(self):
        # Add some data to test persistence.
        self.marionette.execute_script(
            """
            let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
            Ci.nsIBounceTrackingProtection
            );

            bounceTrackingProtection.testAddBounceTrackerCandidate({}, "bouncetracker.net", Date.now() * 10000);
            bounceTrackingProtection.testAddBounceTrackerCandidate({}, "bouncetracker.org", Date.now() * 10000);
            bounceTrackingProtection.testAddBounceTrackerCandidate({ userContextId: 3 }, "tracker.com", Date.now() * 10000);
            // A private browsing entry which must not be persisted across restarts.
            bounceTrackingProtection.testAddBounceTrackerCandidate({ privateBrowsingId: 1 }, "tracker.net", Date.now() * 10000);

            bounceTrackingProtection.testAddUserActivation({}, "example.com", (Date.now() + 5000) * 10000);
            // A private browsing entry which must not be persisted across restarts.
            bounceTrackingProtection.testAddUserActivation({ privateBrowsingId: 1 }, "example.org", (Date.now() + 2000) * 10000);
            """
        )

    def test_state_after_restart(self):
        self.marionette.restart(clean=False, in_app=True)
        bounceTrackerCandidates = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).sort();
            """,
        )
        self.assertEqual(
            len(bounceTrackerCandidates),
            2,
            msg="There should be two entries for default OA",
        )
        self.assertEqual(bounceTrackerCandidates[0], "bouncetracker.net")
        self.assertEqual(bounceTrackerCandidates[1], "bouncetracker.org")

        bounceTrackerCandidates = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetBounceTrackerCandidateHosts({ userContextId: 3 }).sort();
            """,
        )
        self.assertEqual(
            len(bounceTrackerCandidates),
            1,
            msg="There should be only one entry for user context 3",
        )
        self.assertEqual(bounceTrackerCandidates[0], "tracker.com")

        # Unrelated user context should not have any entries.
        bounceTrackerCandidates = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetBounceTrackerCandidateHosts({ userContextId: 4 }).length;
            """,
        )
        self.assertEqual(
            bounceTrackerCandidates,
            0,
            msg="There should be no entries for user context 4",
        )

        # Private browsing entries should not be persisted across restarts.
        bounceTrackerCandidates = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetBounceTrackerCandidateHosts({ privateBrowsingId: 1 }).length;
            """,
        )
        self.assertEqual(
            bounceTrackerCandidates,
            0,
            msg="There should be no entries for private browsing",
        )

        userActivations = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetUserActivationHosts({}).sort();
            """,
        )
        self.assertEqual(
            len(userActivations),
            1,
            msg="There should be only one entry for user activation",
        )
        self.assertEqual(userActivations[0], "example.com")

        # Private browsing entries should not be persisted across restarts.
        userActivations = self.marionette.execute_script(
            """
                let bounceTrackingProtection = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
                Ci.nsIBounceTrackingProtection
                );
                return bounceTrackingProtection.testGetUserActivationHosts({ privateBrowsingId: 1 }).length;
            """,
        )
        self.assertEqual(
            userActivations, 0, msg="There should be no entries for private browsing"
        )
