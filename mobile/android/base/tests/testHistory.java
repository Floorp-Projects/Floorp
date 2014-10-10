package org.mozilla.gecko.tests;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.mozilla.gecko.home.HomePager;

public class testHistory extends AboutHomeTest {
    private View mFirstChild;

    public void testHistory() {
        blockForGeckoReady();

        String url = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String url2 = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        String url3 = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_03_URL);

        inputAndLoadUrl(url);
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        inputAndLoadUrl(url2);
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        inputAndLoadUrl(url3);
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_03_URL);

        openAboutHomeTab(AboutHomeTabs.HISTORY);

        final ListView hList = findListViewWithTag(HomePager.LIST_TAG_HISTORY);
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
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_03_URL);
        verifyUrl(url3);
    }
}
