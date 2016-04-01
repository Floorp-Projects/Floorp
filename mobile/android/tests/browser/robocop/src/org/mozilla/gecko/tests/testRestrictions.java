/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertFalse;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

public class testRestrictions extends UITest {
    public void testRestrictions() {
        GeckoHelper.blockForReady();

        // No restrictions should be enforced when using a normal profile
        for (Restrictable restrictable : Restrictable.values()) {
            if (restrictable == Restrictable.BLOCK_LIST) {
                assertFeatureDisabled(restrictable);
            } else {
                assertFeatureEnabled(restrictable);
            }
        }
    }

    private void assertFeatureEnabled(Restrictable restrictable) {
        fAssertTrue(String.format("Restrictable feature %s is enabled", restrictable.name),
                Restrictions.isAllowed(getActivity(), restrictable)
        );
    }

    private void assertFeatureDisabled(Restrictable restrictable) {
        fAssertFalse(String.format("Restrictable feature %s is disabled", restrictable.name),
                Restrictions.isAllowed(getActivity(), restrictable)
        );
    }
}
