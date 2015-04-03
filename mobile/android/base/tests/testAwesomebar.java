/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

public class testAwesomebar extends BaseTest {
    public void testAwesomebar() {
        blockForGeckoReady();

        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        inputAndLoadUrl(url);

        mDriver.setupScrollHandling();
        // Calculate where we should be dragging.
        int midX = mDriver.getGeckoLeft() + mDriver.getGeckoWidth()/2;
        int midY = mDriver.getGeckoTop() + mDriver.getGeckoHeight()/2;
        int endY = mDriver.getGeckoTop() + mDriver.getGeckoHeight()/10;
        for (int i = 0; i < 10; i++) {
            mActions.drag(midX, midX, midY, endY);
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        verifyUrl(url);
    }
}
