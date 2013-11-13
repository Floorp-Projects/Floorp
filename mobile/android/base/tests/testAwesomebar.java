package org.mozilla.gecko.tests;

public class testAwesomebar extends BaseTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testAwesomebar() {
        blockForGeckoReady();

        String url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
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
