/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Processor implementation to extract the dominant color from the icon and attach it to the icon
 * response object.
 */
public class ColorProcessor implements Processor {
    @Override
    public void process(IconRequest request, IconResponse response) {
        if (response.hasColor()) {
            return;
        }

        response.updateColor(BitmapUtils.getDominantColor(response.getBitmap()));
    }
}
