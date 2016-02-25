/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Intent;

/**
 * Interface for actions run by FeedService.
 */
public interface BaseAction {
    /**
     * Perform this action.
     *
     * @param intent used to start the service.
     */
    void perform(Intent intent);

    /**
     * Does this action require an active network connection?
     */
    boolean requiresNetwork();
}
