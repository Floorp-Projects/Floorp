/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import java.util.Comparator;

/**
 * This comparator implementation compares IconDescriptor objects in order to determine which icon
 * to load first.
 *
 * In general this comparator will try touch icons before favicons (they usually have a higher resolution)
 * and prefers larger icons over smaller ones.
 */
/* package-private */ class IconDescriptorComparator implements Comparator<IconDescriptor> {
    @Override
    public int compare(IconDescriptor lhs, IconDescriptor rhs) {
        if (lhs.getUrl().equals(rhs.getUrl())) {
            // Two descriptors pointing to the same URL are always referencing the same icon. So treat
            // them as equal.
            return 0;
        }

        // First compare the types. We prefer touch icons because they tend to have a higher resolution
        // than ordinary favicons.
        if (lhs.getType() != rhs.getType()) {
            return compareType(lhs, rhs);
        }

        // If one of them is larger than pick the larger icon.
        if (lhs.getSize() != rhs.getSize()) {
            return compareSizes(lhs, rhs);
        }

        // If there's no other way to choose, we prefer container types. They *might* contain
        // an image larger than the size given in the <link> tag.
        final boolean lhsContainer = IconsHelper.isContainerType(lhs.getMimeType());
        final boolean rhsContainer = IconsHelper.isContainerType(rhs.getMimeType());

        if (lhsContainer != rhsContainer) {
            return lhsContainer ? -1 : 1;
        }

        // There's no way to know which icon might be better. Just pick rhs.
        return 1;
    }

    private int compareType(IconDescriptor lhs, IconDescriptor rhs) {
        if (lhs.getType() > rhs.getType()) {
            return -1;
        } else {
            return 1;
        }
    }

    private int compareSizes(IconDescriptor lhs, IconDescriptor rhs) {
        if (lhs.getSize() > rhs.getSize()) {
            return -1;
        } else {
            return 1;
        }
    }
}
