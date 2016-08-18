/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Processor implementation for resizing the loaded icon based on the target size.
 */
public class ResizingProcessor implements Processor {
    @Override
    public void process(IconRequest request, IconResponse response) {
        if (response.isFromMemory()) {
            // This bitmap has been loaded from memory, so it has already gone through the resizing
            // process. We do not want to resize the image every time we hit the memory cache.
            return;
        }

        final Bitmap originalBitmap = response.getBitmap();
        final int size = originalBitmap.getWidth();

        final int targetSize = request.getTargetSize();

        if (size == targetSize) {
            // The bitmap has exactly the size we are looking for.
            return;
        }

        final Bitmap resizedBitmap;

        if (size > targetSize) {
            resizedBitmap = resize(originalBitmap, targetSize);
        } else {
            // Our largest primary is smaller than the desired size. Upscale by a maximum of 2x.
            // 'largestSize' now reflects the maximum size we can upscale to.
            final int largestSize = size * 2;

            if (largestSize > targetSize) {
                // Perfect! We can upscale by less than 2x and reach the needed size. Do it.
                resizedBitmap = resize(originalBitmap, targetSize);
            } else {
                // We don't have enough information to make the target size look non terrible. Best effort:
                resizedBitmap = resize(originalBitmap, largestSize);
            }
        }

        response.updateBitmap(resizedBitmap);

        originalBitmap.recycle();
    }

    @VisibleForTesting Bitmap resize(Bitmap bitmap, int targetSize) {
        try {
            return  Bitmap.createScaledBitmap(bitmap, targetSize, targetSize, true);
        } catch (OutOfMemoryError error) {
            // There's not enough memory to create a resized copy of the bitmap in memory. Let's just
            // use what we have.
            return bitmap;
        }
    }
}
