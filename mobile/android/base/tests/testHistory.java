package org.mozilla.gecko.tests;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

public class testHistory extends AboutHomeTest {
    private View mFirstChild;

    public void testHistory() {
        blockForGeckoReady();

        String url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        String url2 = getAbsoluteUrl("/robocop/robocop_blank_02.html");
        String url3 = getAbsoluteUrl("/robocop/robocop_blank_03.html");

        inputAndLoadUrl(url);
        verifyPageTitle("Browser Blank Page 01");
        inputAndLoadUrl(url2);
        verifyPageTitle("Browser Blank Page 02");
        inputAndLoadUrl(url3);
        verifyPageTitle("Browser Blank Page 03");

        openAboutHomeTab(AboutHomeTabs.MOST_RECENT);

        final ListView hList = findListViewWithTag("most_recent");
        mAsserter.is(waitForNonEmptyListToLoad(hList), true, "list is properly loaded");

        // Click on the history item and wait for the page to load
        // wait for the history list to be populated
        mFirstChild = null;
        boolean success = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                mFirstChild = hList.getChildAt(1);
                if (mFirstChild == null) {
                    return false;
                }
                if (mFirstChild instanceof android.view.ViewGroup) {
                    ViewGroup group = (ViewGroup)mFirstChild;
                    if (group.getChildCount() < 1) {
                        return false;
                    }
                    for (int i = 0; i < group.getChildCount(); i++) {
                        View grandChild = group.getChildAt(i);
                        if (grandChild instanceof android.widget.TextView) {
                            mAsserter.ok(true, "found TextView:", ((android.widget.TextView)grandChild).getText().toString());
                        }
                    }
                } else {
                    mAsserter.dumpLog("first child not a ViewGroup: "+mFirstChild);
                    return false;
                }
                return true;
            }
        }, MAX_WAIT_MS);

        mAsserter.isnot(mFirstChild, null, "Got history item");
        mSolo.clickOnView(mFirstChild);

        // The first item here (since it was just visited) should be a "Switch to tab" item
        // i.e. don't expect a DOMCOntentLoaded event
        verifyPageTitle("Browser Blank Page 03");
        verifyUrl(url3);
    }
}
