/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.preparation;

import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.storage.FailureCache;

import java.util.Iterator;

public class FilterKnownFailureUrls implements Preparer {
    @Override
    public void prepare(IconRequest request) {
        final FailureCache failureCache = FailureCache.get();
        final Iterator<IconDescriptor> iterator = request.getIconIterator();

        while (iterator.hasNext()) {
            final IconDescriptor descriptor = iterator.next();

            if (failureCache.isKnownFailure(descriptor.getUrl())) {
                // Loading from this URL has failed in the past. Do not try again.
                iterator.remove();
            }
        }
    }
}
