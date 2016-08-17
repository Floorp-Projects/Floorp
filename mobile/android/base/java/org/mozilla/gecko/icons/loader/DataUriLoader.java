/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Loader for loading icons from a data URI. This loader will try to decode any data with an
 * "image/*" MIME type.
 *
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs
 */
public class DataUriLoader implements IconLoader {
    @Override
    public IconResponse load(IconRequest request) {
        final String iconUrl = request.getBestIcon().getUrl();

        if (!iconUrl.startsWith("data:image/")) {
            return null;
        }

        LoadFaviconResult loadFaviconResult = FaviconDecoder.decodeDataURI(request.getContext(), iconUrl);
        if (loadFaviconResult == null) {
            return null;
        }

        return IconResponse.create(
                loadFaviconResult.getBestBitmap(request.getTargetSize()));
    }
}
