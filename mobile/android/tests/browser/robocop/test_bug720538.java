package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

public class test_bug720538 extends PixelTest {
    public void test_bug720538() {
        String url = getAbsoluteUrl("/robocop/test_bug720538.html");

        blockForGeckoReady();

        /*
         * for this test, we load the associated test_bug720538.html file. this file has two
         * iframes (painted completely blue - #0000FF) and the rest of the page is the background
         * color, which is #008000 green. When we first render the page there is an iframe in
         * the top-left corner, and when we double-tap on it it should zoom to fill the visible
         * view area leaving a little bit of space on either side. We can test for this by checking
         * a few pixels to ensure that there is some background visible on either side of the iframe.
         * Finally, when we double-tap on it to zoom out again, we need to check that the bottom of
         * the view doesn't have any checkerboarding. We can verify this by getting a few pixels and
         * checking that it is the same as the expected background color of the page, as opposed to
         * the gray shades of the checkerboard.
         */

        PaintedSurface painted = loadAndGetPainted(url);

        try {
            // first we check that the point we want to double-tap (100, 100) is blue, indicating it's inside the iframe
            mAsserter.ispixel(painted.getPixelAt(100, 100), 0, 0, 0xFF, "Ensuring double-tap point is in the iframe");
        } finally {
            painted.close();
        }

        // do the double tap and wait for the double-tap animation to finish. we assume the animation is done
        // when we find a 500ms period with no paint events that occurs after at least one paint event.
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());
        meh.doubleTap(100, 100);
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();

        try {
            // check a few points to ensure that we did a good zoom-to-block on the iframe. this checks that
            // the background color is visible on the left and right edges of the viewport, but the iframe is
            // visible in between those edges
            mAsserter.ispixel(painted.getPixelAt(0, 100), 0, 0x80, 0, "Checking page background to the left of the iframe");
            mAsserter.ispixel(painted.getPixelAt(50, 100), 0, 0, 0xFF, "Checking for iframe a few pixels from the left edge");
            mAsserter.ispixel(painted.getPixelAt(mDriver.getGeckoWidth() - 51, 100), 0, 0, 0xFF, "Checking for iframe a few pixels from the right edge");
            mAsserter.ispixel(painted.getPixelAt(mDriver.getGeckoWidth() - 1, 100), 0, 0x80, 0, "Checking page background the right of the iframe");
        } finally {
            painted.close();
        }

        // now we do double-tap again to zoom out and wait for the animation to finish, as before
        paintExpecter = mActions.expectPaint();
        meh.doubleTap(mDriver.getGeckoWidth() / 2, 100);
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();

        try {
            // and now we check a pixel at the bottom of the view to ensure that we have the page
            // background and not some checkerboarding. use the second-last row of pixels instead of
            // the last row because the last row is subject to rounding and clipping errors
            for (int y = 2; y < 10; y++) {
                for (int x = 0; x < 10; x++) {
                    mAsserter.dumpLog("Pixel at " + x + ", " + (mDriver.getGeckoHeight() - y) + ": " + Integer.toHexString(painted.getPixelAt(x, mDriver.getGeckoHeight() - y)));
                }
            }
            mAsserter.ispixel(painted.getPixelAt(0, mDriver.getGeckoHeight() - 2), 0, 0x80, 0, "Checking bottom-left corner of viewport");
        } finally {
            painted.close();
        }
    }
}
