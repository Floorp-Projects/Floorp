/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.loader.IconGenerator;

/**
 * Substitutes a generated image for the given icon if it doesn't not meet the minimum size requirement.
 *
 * Ideally, we would discard images while we're loading them: if we found an icon that was too small, we
 * would have the opportunity to search other places before generating an icon and we wouldn't duplicate
 * the generate icon call. However, this turned out to be non-trivial: a single icon will appear as two
 * different sizes when given to the loader, the original size the first time it's loaded and the size after
 * {@link ResizingProcessor} the second time, when it's loaded from the cache. It turned out to be much simpler
 * to enforce the requirement that...
 *
 * This processor is expected to be called after {@link ResizingProcessor}.
 */
public class MinimumSizeProcessor implements Processor {

    @Override
    public void process(final IconRequest request, final IconResponse response) {
        // We expect that this bitmap has already been scaled by ResizingProcessor.
        if (response.getBitmap().getWidth() >= request.getMinimumSizePxAfterScaling()) {
            return;
        }

        // This is fragile: ideally, we can return the generated response but instead we're mutating the argument.
        final IconResponse generatedResponse = IconGenerator.generate(request.getContext(), request.getPageUrl());
        response.updateBitmap(generatedResponse.getBitmap());
        response.updateColor(generatedResponse.getColor());
    }
}
