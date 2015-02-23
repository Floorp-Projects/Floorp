/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

/**
 * Basic fling correctness test.
 * - Loads a page and verifies it draws
 * - Drags page upwards by 200 pixels to get ready for a fling
 * - Fling the page downwards so we get back to the top and verify.
 */
public class testFlingCorrectness extends PixelTest {
    public void testFlingCorrectness() {
        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BOXES_URL);

        MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());

        blockForGeckoReady();

        // load page and check we're at 0,0
        loadAndVerifyBoxes(url);

        // drag page upwards by 200 pixels (use two drags instead of one in case
        // the screen size is small)
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        meh.dragSync(10, 150, 10, 50);
        meh.dragSync(10, 150, 10, 50);
        PaintedSurface painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 200);
        } finally {
            painted.close();
        }

        // now fling page downwards using a 100-pixel drag but a velocity of 15px/sec, so that
        // we scroll the full 200 pixels back to the top of the page
        paintExpecter = mActions.expectPaint();
        meh.flingSync(10, 50, 10, 150, 15);
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 0);
        } finally {
            painted.close();
        }
    }
}
