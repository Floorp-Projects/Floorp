/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import android.support.annotation.Nullable;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Generic interface for classes that can load icons.
 */
public interface IconLoader {
    /**
     * Loads the icon for this request or returns null if this loader can't load an icon for this
     * request or just failed this time.
     */
    @Nullable
    IconResponse load(IconRequest request);
}
