/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.preparation;

import org.mozilla.gecko.icons.IconRequest;

/**
 * Generic interface for a class "preparing" a request before we try to load icons. A class
 * implementing this interface can modify the request (e.g. filter or add icon URLs).
 */
public interface Preparer {
    /**
     * Inspects or modifies the request before any icon is loaded.
     */
    void prepare(IconRequest request);
}
