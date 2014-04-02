package org.mozilla.gecko.tests;

/**
 * A basic page load test.
 * - loads a page
 * - verifies it rendered properly
 * - verifies the displayed url is correct
 */
public class testLoad extends PixelTest {
    public void testLoad() {
        String url = getAbsoluteUrl("/robocop/robocop_boxes.html");

        blockForGeckoReady();

        loadAndVerifyBoxes(url);

        verifyUrl(url);
    }
}
