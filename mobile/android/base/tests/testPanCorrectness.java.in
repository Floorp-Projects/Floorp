#filter substitution
package @ANDROID_PACKAGE_NAME@.tests;

import @ANDROID_PACKAGE_NAME@.*;

/**
 * A basic panning correctness test.
 * - Loads a page and verifies it draws
 * - drags page upwards by 100 pixels and verifies it draws
 * - drags page leftwards by 100 pixels and verifies it draws
 */
public class testPanCorrectness extends PixelTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testPanCorrectness() {
        String url = getAbsoluteUrl("/robocop/robocop_boxes.html");

        MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());

        blockForGeckoReady();

        // load page and check we're at 0,0
        loadAndVerifyBoxes(url);

        // drag page upwards by 100 pixels
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        meh.dragSync(10, 150, 10, 50);
        PaintedSurface painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 100);
        } finally {
            painted.close();
        }

        // drag page leftwards by 100 pixels
        paintExpecter = mActions.expectPaint();
        meh.dragSync(150, 10, 50, 10);
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 100, 100);
        } finally {
            painted.close();
        }
    }
}
