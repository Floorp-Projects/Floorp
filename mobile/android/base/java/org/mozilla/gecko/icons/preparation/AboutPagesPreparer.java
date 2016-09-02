/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.preparation;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.util.GeckoJarReader;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Preparer implementation for adding the omni.ja URL for internal about: pages.
 */
public class AboutPagesPreparer implements Preparer {
    private Set<String> aboutUrls;

    public AboutPagesPreparer() {
        aboutUrls = new HashSet<>();

        Collections.addAll(aboutUrls, AboutPages.DEFAULT_ICON_PAGES);
    }

    @Override
    public void prepare(IconRequest request) {
        if (aboutUrls.contains(request.getPageUrl())) {
            final String iconUrl = GeckoJarReader.getJarURL(request.getContext(), "chrome/chrome/content/branding/favicon64.png");

            request.modify()
                    .icon(IconDescriptor.createLookupIcon(iconUrl))
                    .deferBuild();
        }
    }
}
