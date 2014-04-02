package org.mozilla.gecko.tests;


/**
 * A panning performance test.
 * Drags the page a bunch of times and measures the frames per second
 * that fennec draws at.
 */
public class testPan extends PixelTest {
    @Override
    protected Type getTestType() {
        return Type.TALOS;
    }

    public void testPan() {
        String url = getAbsoluteUrl("/startup_test/fennecmark/wikipedia.html");

        blockForGeckoReady();

        loadAndPaint(url);

        mDriver.setupScrollHandling();

        // Setup scrolling coordinates.
        int midX = mDriver.getGeckoLeft() + mDriver.getGeckoWidth()/2;
        int midY = mDriver.getGeckoTop() + mDriver.getGeckoHeight()/2;
        int endY = mDriver.getGeckoTop() + mDriver.getGeckoHeight()/10;

        mDriver.startFrameRecording();

        int i = 0;
        // Scroll a thousand times or until the end of the page.
        do {
            mActions.drag(midX, midX, midY, endY);
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            i++;
        } while (i < 1000 && mDriver.getScrollHeight() + 2 * mDriver.getHeight() < mDriver.getPageHeight());
        // asserter.ok(i < 1000, "Less than 1000", "Should take less than 1000 drags to get to bottom of the page.");

        int frames = mDriver.stopFrameRecording();
        mAsserter.dumpLog("__start_report" + Integer.toString(frames) + "__end_report");
        long msecs = System.currentTimeMillis();
        mAsserter.dumpLog("__startTimestamp" + msecs + "__endTimestamp");
    }
}
