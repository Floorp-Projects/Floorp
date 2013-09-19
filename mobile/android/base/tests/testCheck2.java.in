#filter substitution
package @ANDROID_PACKAGE_NAME@.tests;

public class testCheck2 extends PixelTest {
    @Override
    protected int getTestType() {
        return TEST_TALOS;
    }

    public void testCheck2() {
        String url = getAbsoluteUrl("/startup_test/fennecmark/cnn/cnn.com/index.html");

        blockForGeckoReady();
        loadAndPaint(url);

        mDriver.setupScrollHandling();

        /*
         * for this test, we load the timecube page, and replay a recorded sequence of events
         * that is a user panning/zooming around the page. specific things in the sequence
         * include:
         * - scroll on one axis followed by scroll on another axis
         * - pinch zoom (in and out)
         * - double-tap zoom (in and out)
         * - multi-fling panning with different velocities on each fling
         *
         * this checkerboarding metric is going to be more of a "functional" style test than
         * a "unit" style test; i.e. it covers a little bit of a lot of things to measure
         * overall performance, but doesn't really allow identifying which part is slow.
         */

        MotionEventReplayer mer = new MotionEventReplayer(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop(),
                mDriver.getGeckoWidth(), mDriver.getGeckoHeight());

        float completeness = 0.0f;
        mDriver.startCheckerboardRecording();
        // replay the events
        try {
            mer.replayEvents(getAsset("testcheck2-motionevents"));
            // give it some time to draw any final frames
            Thread.sleep(1000);
            completeness = mDriver.stopCheckerboardRecording();
        } catch (Exception e) {
            mAsserter.ok(false, "Exception while replaying events", e.toString());
        }

        mAsserter.dumpLog("__start_report" + completeness + "__end_report");
        System.out.println("Completeness score: " + completeness);
        long msecs = System.currentTimeMillis();
        mAsserter.dumpLog("__startTimestamp" + msecs + "__endTimestamp");
    }
}
