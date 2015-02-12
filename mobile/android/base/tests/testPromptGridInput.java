/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;

public class testPromptGridInput extends BaseTest {
    protected int index = 1;
    public void testPromptGridInput() {
        blockForGeckoReady();

        test(1);

        testGridItem("Icon 1");
        testGridItem("Icon 2");
        testGridItem("Icon 3");
        testGridItem("Icon 4");
        testGridItem("Icon 5");
        testGridItem("Icon 6");
        testGridItem("Icon 7");
        testGridItem("Icon 8");
        testGridItem("Icon 9");
        testGridItem("Icon 10");
        testGridItem("Icon 11");

        mSolo.clickOnText("Icon 11");
        mSolo.clickOnText("OK");

        mAsserter.ok(waitForText("PASS"), "test passed", "PASS");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
    }

    public void testGridItem(String title) {
        // Force the list to scroll if necessary
        mSolo.waitForText(title, 1, 500, true);
        mAsserter.ok(waitForText(title), "Found grid item", title);
    }

    public void test(final int num) {
        // Load about:blank between each test to ensure we reset state
        loadUrl(mStringHelper.ABOUT_BLANK_URL);
        mAsserter.ok(waitForText(mStringHelper.ABOUT_BLANK_URL), "Loaded blank page",
                mStringHelper.ABOUT_BLANK_URL);

        loadUrl("chrome://roboextender/content/robocop_prompt_gridinput.html#test" + num);
    }
}
