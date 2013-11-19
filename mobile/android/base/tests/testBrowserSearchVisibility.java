package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;
import android.app.Activity;
import android.content.Context;
import android.support.v4.app.Fragment;
import android.view.View;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.widget.TextView;
import java.lang.RuntimeException;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Test for browser search visibility.
 * Sends queries from url bar input and verifies that browser search
 * visibility is correct.
 */
public class testBrowserSearchVisibility extends BaseTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testSearchSuggestions() {
        blockForGeckoReady();

        focusUrlBar();

        // search should not be visible when editing mode starts
        assertBrowserSearchVisibility(false);

        mActions.sendKeys("a");

        // search should be visible when entry is not empty
        assertBrowserSearchVisibility(true);

        mActions.sendKeys("b");

        // search continues to be visible when more text is added
        assertBrowserSearchVisibility(true);

        mActions.sendKeyCode(KeyEvent.KEYCODE_DEL);

        // search continues to be visible when not all text is deleted
        assertBrowserSearchVisibility(true);

        mActions.sendKeyCode(KeyEvent.KEYCODE_DEL);

        // search should not be visible, entry is empty now
        assertBrowserSearchVisibility(false);
    }

    private void assertBrowserSearchVisibility(final boolean isVisible) {
        waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                final Fragment browserSearch = getBrowserSearch();

                // The fragment should not be present at all. Testing if the
                // fragment is present but has no defined view is not a valid
                // state.
                if (browserSearch == null)
                    return !isVisible;

                final View v = browserSearch.getView();
                if (isVisible && v != null && v.getVisibility() == View.VISIBLE)
                    return true;

                return false;
            }
        }, 5000);
    }
}

