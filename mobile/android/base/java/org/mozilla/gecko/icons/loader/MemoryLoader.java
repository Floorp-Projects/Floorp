/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.storage.MemoryStorage;

/**
 * Loader implementation for loading icons from an in-memory cached (Implemented by MemoryStorage).
 */
public class MemoryLoader implements IconLoader {
    private final MemoryStorage storage;

    public MemoryLoader() {
        storage = MemoryStorage.get();
    }

    @Override
    public IconResponse load(IconRequest request) {
        if (request.shouldSkipMemory()) {
            return null;
        }

        final String iconUrl = request.getBestIcon().getUrl();
        return storage.getIcon(iconUrl);
    }
}
