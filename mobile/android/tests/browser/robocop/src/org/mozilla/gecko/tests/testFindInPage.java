/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fFail;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import com.robotium.solo.Condition;

public class testFindInPage extends JavascriptTest implements BundleEventListener {
    private static final int WAIT_FOR_CONDITION_MS = 3000;

    protected Element next, close;

    public testFindInPage() {
        super("testFindInPage.js");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Test:FindInPage".equals(event)) {
            final String text = message.getString("text");
            final int nrOfMatches = message.getInt("nrOfMatches");
            findText(text, nrOfMatches);

        } else if ("Test:CloseFindInPage".equals(event)) {
            close.click();
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                                                                  "Test:FindInPage",
                                                                  "Test:CloseFindInPage");
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                                                                    "Test:FindInPage",
                                                                    "Test:CloseFindInPage");
    }

    public void findText(String text, int nrOfMatches){
        selectMenuItem(mStringHelper.FIND_IN_PAGE_LABEL);
        close = mDriver.findElement(getActivity(), R.id.find_close);
        boolean success = waitForCondition ( new Condition() {
            @Override
            public boolean isSatisfied() {
                next = mDriver.findElement(getActivity(), R.id.find_next);
                if (next != null) {
                    return true;
                } else {
                    return false;
                }
            }
        }, WAIT_FOR_CONDITION_MS);
        mAsserter.ok(success, "Looking for the next search match button in the Find in Page UI", "Found the next match button");

        // TODO: Find a better way to wait and then enter the text
        // Without the sleep this seems to work but the actions are not updated in the UI
        mSolo.sleep(500);

        mActions.sendKeys(text);
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);

        // Advance a few matches to scroll the page
        for (int i=1;i < nrOfMatches;i++) {
            success = waitForCondition ( new Condition() {
                @Override
                public boolean isSatisfied() {
                    if (next.click()) {
                        return true;
                    } else {
                        return false;
                    }
                }
            }, WAIT_FOR_CONDITION_MS);
            mSolo.sleep(500); // TODO: Find a better way to wait here because waitForCondition is not enough
            mAsserter.ok(success, "Checking if the next button was clicked", "button was clicked");
        }
    }
}
