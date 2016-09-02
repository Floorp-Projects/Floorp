/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.storage.MemoryStorage;

public class MemoryProcessor implements Processor {
    private final MemoryStorage storage;

    public MemoryProcessor() {
        storage = MemoryStorage.get();
    }

    @Override
    public void process(IconRequest request, IconResponse response) {
        if (request.shouldSkipMemory() || request.getIconCount() == 0 || response.isGenerated()) {
            // Do not cache this icon in memory if we should skip the memory cache or if this icon
            // has been generated. We can re-generate it if needed.
            return;
        }

        final String iconUrl = request.getBestIcon().getUrl();

        if (iconUrl.startsWith("data:image/")) {
            // The image data is encoded in the URL. It doesn't make sense to store the URL and the
            // bitmap in cache.
            return;
        }

        storage.putMapping(request, iconUrl);
        storage.putIcon(iconUrl, response);
    }
}
