package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

/**
 * Basic test to check bounce-back from overscroll.
 * - Load the page and verify it draws
 * - Drag page downwards by 100 pixels into overscroll, verify it snaps back.
 * - Drag page rightwards by 100 pixels into overscroll, verify it snaps back.
 */
public class testOverscroll extends PixelTest {
    public void testOverscroll() {
        String url = getAbsoluteUrl("/robocop/robocop_boxes.html");

        MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());

        blockForGeckoReady();

        // load page and check we're at 0,0
        loadAndVerifyBoxes(url);

        // drag page downwards by 100 pixels so that it goes into overscroll.
        // wait for it to bounce back and check we're at the right spot.
        // the screen size is small). Note that since we only go into overscroll
        // and back this should NOT trigger a gecko-paint
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        meh.dragSync(10, 50, 10, 150);
        PaintedSurface painted = waitWithNoPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 0);
        } finally {
            painted.close();
        }

        // drag page rightwards to go into overscroll on the left. let it bounce and verify.
        paintExpecter = mActions.expectPaint();
        meh.dragSync(50, 10, 150, 10);
        painted = waitWithNoPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 0);
        } finally {
            painted.close();
        }
    }
}
