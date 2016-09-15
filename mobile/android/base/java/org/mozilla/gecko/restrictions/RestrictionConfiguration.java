/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

/**
 * Interface for classes that Restrictions will delegate to for making decisions.
 */
public interface RestrictionConfiguration {
    /**
     * Is the user allowed to perform this action?
     */
    boolean isAllowed(Restrictable restrictable);

    /**
     * Is the user allowed to load the given URL?
     */
    boolean canLoadUrl(String url);

    /**
     * Is this user restricted in any way?
     */
    boolean isRestricted();

    /**
     * Update restrictions if needed.
     */
    void update();
}
