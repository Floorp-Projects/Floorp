/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

abstract class PixelTest extends BaseTest {
    private static final long PAINT_CLEAR_DELAY = 10000; // milliseconds

    protected final PaintedSurface loadAndGetPainted(String url) {
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        loadUrlAndWait(url);
        verifyHomePagerHidden();
        paintExpecter.blockUntilClear(PAINT_CLEAR_DELAY);
        paintExpecter.unregisterListener();
        PaintedSurface p = mDriver.getPaintedSurface();
        if (p == null) {
            mAsserter.ok(p != null, "checking that painted surface loaded", 
                 "painted surface loaded");
        }
        return p;
    }

    protected final void loadAndPaint(String url) {
        PaintedSurface painted = loadAndGetPainted(url);
        painted.close();
    }

    protected final PaintedSurface reloadAndGetPainted() {
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();

        mActions.sendSpecialKey(Actions.SpecialKey.MENU);
        waitForText(mStringHelper.RELOAD_LABEL);
        mSolo.clickOnText(mStringHelper.RELOAD_LABEL);

        paintExpecter.blockUntilClear(PAINT_CLEAR_DELAY);
        paintExpecter.unregisterListener();
        PaintedSurface p = mDriver.getPaintedSurface();
        if (p == null) {
            mAsserter.ok(p != null, "checking that painted surface loaded", 
                 "painted surface loaded");
        }
        return p;
    }

    protected final void reloadAndPaint() {
        PaintedSurface painted = reloadAndGetPainted();
        painted.close();
    }

    protected final PaintedSurface waitForPaint(Actions.RepeatedEventExpecter expecter) {
        expecter.blockUntilClear(PAINT_CLEAR_DELAY);
        PaintedSurface p = mDriver.getPaintedSurface();
        if (p == null) {
            mAsserter.ok(p != null, "checking that painted surface loaded", 
                 "painted surface loaded");
        }
        return p;
    }

    protected final PaintedSurface waitWithNoPaint(Actions.RepeatedEventExpecter expecter) {
        try {
            Thread.sleep(PAINT_CLEAR_DELAY);
        } catch (InterruptedException ie) {
            ie.printStackTrace();
        }
        mAsserter.is(expecter.eventReceived(), false, "Checking gecko didn't draw unnecessarily");
        PaintedSurface p = mDriver.getPaintedSurface();
        if (p == null) {
            mAsserter.ok(p != null, "checking that painted surface loaded", 
                 "painted surface loaded");
        }
        return p;
    }

    // this matches the algorithm in robocop_boxes.html
    protected final int[] getBoxColorAt(int x, int y) {
        int r = ((int)Math.floor(x / 3) % 256);
        r = r & 0xF8;
        int g = (x + y) % 256;
        g = g & 0xFC;
        int b = ((int)Math.floor(y / 3) % 256);
        b = b & 0xF8;
        return new int[] { r, g, b };
    }

    /**
     * Checks the top-left corner of the visible area of the page is at (x,y) of robocop_boxes.html.
     */
    protected final void checkScrollWithBoxes(PaintedSurface painted, int x, int y) {
        int[] color = getBoxColorAt(x, y);
        mAsserter.ispixel(painted.getPixelAt(0, 0), color[0], color[1], color[2], "Pixel at 0, 0");
        color = getBoxColorAt(x + 100, y);
        mAsserter.ispixel(painted.getPixelAt(100, 0), color[0], color[1], color[2], "Pixel at 100, 0");
        color = getBoxColorAt(x, y + 100);
        mAsserter.ispixel(painted.getPixelAt(0, 100), color[0], color[1], color[2], "Pixel at 0, 100");
        color = getBoxColorAt(x + 100, y + 100);
        mAsserter.ispixel(painted.getPixelAt(100, 100), color[0], color[1], color[2], "Pixel at 100, 100");
    }

    /**
     * Loads the robocop_boxes.html file and verifies that we are positioned at (0,0) on it.
     * @param url URL of the robocop_boxes.html file.
     * @return The painted surface after rendering the file.
     */
    protected final void loadAndVerifyBoxes(String url) {
        PaintedSurface painted = loadAndGetPainted(url);
        try {
            checkScrollWithBoxes(painted, 0, 0);
        } finally {
            painted.close();
        }
    }
}
