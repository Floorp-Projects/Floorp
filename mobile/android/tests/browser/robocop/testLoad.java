/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

/**
 * A basic page load test.
 * - loads a page
 * - verifies it rendered properly
 * - verifies the displayed url is correct
 */
public class testLoad extends PixelTest {
    public void testLoad() {
        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BOXES_URL);

        blockForGeckoReady();

        loadAndVerifyBoxes(url);

        verifyUrl(url);
    }
}
