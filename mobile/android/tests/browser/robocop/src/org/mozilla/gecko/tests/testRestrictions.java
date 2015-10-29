/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.restrictions.Restriction;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

public class testRestrictions extends UITest {
    public void testRestrictions() {
        GeckoHelper.blockForReady();

        // No restrictions should be enforced when using a normal profile
        for (Restriction restriction : Restriction.values()) {
            fAssertTrue(String.format("Restriction %s is not enforced", restriction.name),
                RestrictedProfiles.isAllowed(getActivity(), restriction)
            );
        }
    }
}
