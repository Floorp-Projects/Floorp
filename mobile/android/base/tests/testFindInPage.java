package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.PaintedSurface;
import org.mozilla.gecko.R;

public class testFindInPage extends PixelTest {
    private static final int WAIT_FOR_TEST = 3000;
    protected Element next, close;
    int height,width;

    public void testFindInPage() {
        blockForGeckoReady();
        String url = getAbsoluteUrl(StringHelper.ROBOCOP_TEXT_PAGE_URL);
        loadAndPaint(url);

        height = mDriver.getGeckoHeight()/8;
        width = mDriver.getGeckoWidth()/2;

        // Search that does not find the term and therefor should not pan the page
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        findText("Robocoop", 3); // This will be close enough to existing text to test that search finds just what it should
        PaintedSurface painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            mAsserter.ispixel(painted.getPixelAt(width,height), 255, 0, 0, "Pixel at " + String.valueOf(width) + "," + String.valueOf(height));
        } finally {
            painted.close();
        }

        // Search that finds matches and therefor pans the page
        paintExpecter = mActions.expectPaint();
        findText("Robocop", 3);
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            mAsserter.isnotpixel(painted.getPixelAt(width,height), 255, 0, 0, "Pixel at " + String.valueOf(width) + "," + String.valueOf(height));
        } finally {
            painted.close();
        }
    }

    public void findText(String text, int nrOfMatches){
        selectMenuItem(StringHelper.FIND_IN_PAGE_LABEL);
        close = mDriver.findElement(getActivity(), R.id.find_close);
        boolean success = waitForTest ( new BooleanTest() {
            @Override
            public boolean test() {
                next = mDriver.findElement(getActivity(), R.id.find_next);
                if (next != null) {
                    return true;
                } else {
                    return false;
                }
            }
        }, WAIT_FOR_TEST);
        mAsserter.ok(success, "Looking for the next search match button in the Find in Page UI", "Found the next match button");

        // TODO: Find a better way to wait and then enter the text
        // Without the sleep this seems to work but the actions are not updated in the UI
        mSolo.sleep(500);

        mActions.sendKeys(text);
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);

        // Advance a few matches to scroll the page
        for (int i=1;i < nrOfMatches;i++) {
            success = waitForTest ( new BooleanTest() {
                @Override
                public boolean test() {
                    if (next.click()) {
                        return true;
                    } else {
                        return false;
                    }
                }
            }, WAIT_FOR_TEST);
            mSolo.sleep(500); // TODO: Find a better way to wait here because waitForTest is not enough
            mAsserter.ok(success, "Checking if the next button was clicked", "button was clicked");
        }
        close.click(); // Close find in page bar
    }
}
