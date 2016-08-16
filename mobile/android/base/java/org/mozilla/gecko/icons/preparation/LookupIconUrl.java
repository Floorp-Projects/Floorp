/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.preparation;

import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.storage.DiskStorage;
import org.mozilla.gecko.icons.storage.MemoryStorage;

/**
 * Preparer implementation to lookup the icon URL for the page URL in the request. This class tries
 * to locate the icon URL by looking through previously stored mappings on disk and in memory.
 */
public class LookupIconUrl implements Preparer {
    @Override
    public void prepare(IconRequest request) {
        if (lookupFromMemory(request)) {
            return;
        }

        lookupFromDisk(request);
    }

    private boolean lookupFromMemory(IconRequest request) {
        final String iconUrl = MemoryStorage.get()
                .getMapping(request.getPageUrl());

        if (iconUrl != null) {
            request.modify()
                    .icon(IconDescriptor.createLookupIcon(iconUrl))
                    .deferBuild();

            return true;
        }

        return false;
    }

    private boolean lookupFromDisk(IconRequest request) {
        final String iconUrl = DiskStorage.get(request.getContext())
                .getMapping(request.getPageUrl());

        if (iconUrl != null) {
            request.modify()
                    .icon(IconDescriptor.createLookupIcon(iconUrl))
                    .deferBuild();

            return true;
        }

        return false;
    }
}
