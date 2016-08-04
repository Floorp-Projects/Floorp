/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

/**
 * Default implementation of RestrictionConfiguration interface. Used whenever no restrictions are enforced for the
 * current profile.
 */
public class DefaultConfiguration implements RestrictionConfiguration {
    @Override
    public boolean isAllowed(Restrictable restrictable) {
        if (restrictable == Restrictable.BLOCK_LIST) {
            return false;
        }

        return true;
    }

    @Override
    public boolean canLoadUrl(String url) {
        return true;
    }

    @Override
    public boolean isRestricted() {
        return false;
    }

    @Override
    public void update() {}
}
