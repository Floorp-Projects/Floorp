/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.preparation;

import android.text.TextUtils;

import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconsHelper;

import java.util.Iterator;

/**
 * Preparer implementation to filter unknown MIME types to avoid loading images that we cannot decode.
 */
public class FilterMimeTypes implements Preparer {
    @Override
    public void prepare(IconRequest request) {
        final Iterator<IconDescriptor> iterator = request.getIconIterator();

        while (iterator.hasNext()) {
            final IconDescriptor descriptor = iterator.next();
            final String mimeType = descriptor.getMimeType();

            if (TextUtils.isEmpty(mimeType)) {
                // We do not have a MIME type for this icon, so we cannot know in advance if we are able
                // to decode it. Let's just continue.
                return;
            }

            if (!IconsHelper.canDecodeType(mimeType)) {
                iterator.remove();
            }
        }
    }
}
