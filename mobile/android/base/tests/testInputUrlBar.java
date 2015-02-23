/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

import android.widget.EditText;

/**
 * Basic test of text editing within the editing mode.
 * - Enter some text, move the cursor around, and modifying some text.
 * - Check that all edit entry text is selected after switching about:home tabs.
 */
public final class testInputUrlBar extends BaseTest {
    private Element mUrlBarEditElement;
    private EditText mUrlBarEditView;

    public void testInputUrlBar() {
        blockForGeckoReady();

        startEditingMode();
        assertUrlBarText(mStringHelper.ABOUT_HOME_URL);

        // Avoid any auto domain completion by using a prefix that matches
        //  nothing, including about: pages
        mActions.sendKeys("zy");
        assertUrlBarText("zy");

        mActions.sendKeys("cd");
        assertUrlBarText("zycd");

        mActions.sendSpecialKey(Actions.SpecialKey.LEFT);
        mActions.sendSpecialKey(Actions.SpecialKey.LEFT);

        // Inserting "" should not do anything.
        mActions.sendKeys("");
        assertUrlBarText("zycd");

        mActions.sendKeys("ef");
        assertUrlBarText("zyefcd");

        mActions.sendSpecialKey(Actions.SpecialKey.RIGHT);
        mActions.sendKeys("gh");
        assertUrlBarText("zyefcghd");

        final EditText editText = mUrlBarEditView;
        runOnUiThreadSync(new Runnable() {
            @Override
            public void run() {
                // Select "ef"
                editText.setSelection(2);
            }
        });
        mActions.sendKeys("op");
        assertUrlBarText("zyopefcghd");

        runOnUiThreadSync(new Runnable() {
            @Override
            public void run() {
                // Select "cg"
                editText.setSelection(6, 8);
            }
        });
        mActions.sendKeys("qr");
        assertUrlBarText("zyopefqrhd");

        runOnUiThreadSync(new Runnable() {
            @Override
            public void run() {
                // Select "op"
                editText.setSelection(4,2);
            }
        });
        mActions.sendKeys("st");
        assertUrlBarText("zystefqrhd");

        runOnUiThreadSync(new Runnable() {
            @Override
            public void run() {
                editText.selectAll();
            }
        });
        mActions.sendKeys("uv");
        assertUrlBarText("uv");

        // Dismiss the VKB
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);

        // Dismiss editing mode
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);

        waitForText(mStringHelper.TITLE_PLACE_HOLDER);

        // URL bar should have forgotten about "uv" text.
        startEditingMode();
        assertUrlBarText(mStringHelper.ABOUT_HOME_URL);

        int width = mDriver.getGeckoWidth() / 2;
        int y = mDriver.getGeckoHeight() / 2;

        // Slide to the right, force URL bar entry to lose input focus
        mActions.drag(width, 0, y, y);

        // Select text and replace the content
        mSolo.clickOnView(mUrlBarEditView);
        mActions.sendKeys("yz");

        String yz = getUrlBarText();
        mAsserter.ok("yz".equals(yz), "Is the URL bar text \"yz\"?", yz);
    }

    private void startEditingMode() {
        focusUrlBar();

        mUrlBarEditElement = mDriver.findElement(getActivity(), R.id.url_edit_text);
        final int id = mUrlBarEditElement.getId();
        mUrlBarEditView = (EditText) getActivity().findViewById(id);
    }

    private String getUrlBarText() {
        final String elementText = mUrlBarEditElement.getText();
        final String editText = mUrlBarEditView.getText().toString();
        mAsserter.is(editText, elementText, "Does URL bar editText == elementText?");

        return editText;
    }

    private void assertUrlBarText(String expectedText) {
        String actualText = getUrlBarText();
        mAsserter.is(actualText, expectedText, "Does URL bar actualText == expectedText?");
    }
}
