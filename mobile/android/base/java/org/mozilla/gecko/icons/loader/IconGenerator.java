/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import org.mozilla.gecko.favicons.FaviconGenerator;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * This loader will generate an icon in case no icon could be loaded. In order to do so this needs
 * to be the last loader that will be tried.
 */
public class IconGenerator implements IconLoader {
    @Override
    public IconResponse load(IconRequest request) {
        if (request.getIconCount() > 1) {
            // There are still other icons to try. We will only generate an icon if there's only one
            // icon left and all previous loaders have failed (assuming this is the last one).
            return null;
        }

        final FaviconGenerator.IconWithColor iconWithColor = FaviconGenerator.generate(
                request.getContext(), request.getPageUrl());

        return IconResponse.createGenerated(iconWithColor.bitmap, iconWithColor.color);
    }
}
