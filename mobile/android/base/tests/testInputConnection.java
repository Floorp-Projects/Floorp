package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertSelection;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertSelectionAt;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertText;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertTextAndSelection;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertTextAndSelectionAt;

import org.mozilla.gecko.tests.components.GeckoViewComponent.InputConnectionTest;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Tests the proper operation of GeckoInputConnection
 */
public class testInputConnection extends UITest {

    private static final String INITIAL_TEXT = "foo";

    public void testInputConnection() throws InterruptedException {
        GeckoHelper.blockForReady();

        final String url = StringHelper.ROBOCOP_INPUT_URL + "#" + INITIAL_TEXT;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(StringHelper.ROBOCOP_INPUT_TITLE, url);

        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest());
    }

    private class BasicInputConnectionTest implements InputConnectionTest {
        @Override
        public void test(InputConnection ic, EditorInfo info) {
            // Test initial text provided by the hash in the test page URL
            assertText("Initial text matches URL hash", ic, INITIAL_TEXT);

            // Test setSelection
            ic.setSelection(0, 3);
            assertSelection("Can set selection to range", ic, 0, 3);
            ic.setSelection(-3, 6);
            // Test both forms of assert
            assertTextAndSelection("Can handle invalid range", ic, INITIAL_TEXT, 0, 3);
            ic.setSelection(3, 3);
            assertSelectionAt("Can collapse selection", ic, 3);
            ic.setSelection(4, 4);
            assertTextAndSelectionAt("Can handle invalid cursor", ic, INITIAL_TEXT, 3);

            // Test commitText
            ic.commitText("", 10); // Selection past end of new text
            assertTextAndSelectionAt("Can commit empty text", ic, "foo", 3);
            ic.commitText("bar", 1); // Selection at end of new text
            assertTextAndSelectionAt("Can commit text (select after)", ic, "foobar", 6);
            ic.commitText("foo", -1); // Selection at start of new text
            assertTextAndSelectionAt("Can commit text (select before)", ic, "foobarfoo", 5);

            // Test deleteSurroundingText
            ic.deleteSurroundingText(1, 0);
            assertTextAndSelectionAt("Can delete text before", ic, "foobrfoo", 4);
            ic.deleteSurroundingText(1, 1);
            assertTextAndSelectionAt("Can delete text before/after", ic, "foofoo", 3);
            ic.deleteSurroundingText(0, 10);
            assertTextAndSelectionAt("Can delete text after", ic, "foo", 3);
            ic.deleteSurroundingText(0, 0);
            assertTextAndSelectionAt("Can delete empty text", ic, "foo", 3);

            // Test setComposingText
            ic.setComposingText("foo", 1);
            assertTextAndSelectionAt("Can start composition", ic, "foofoo", 6);
            ic.setComposingText("", 1);
            assertTextAndSelectionAt("Can set empty composition", ic, "foo", 3);
            ic.setComposingText("bar", 1);
            assertTextAndSelectionAt("Can update composition", ic, "foobar", 6);

            // Test finishComposingText
            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composition", ic, "foobar", 6);

            // Test getTextBeforeCursor
            fAssertEquals("Can retrieve text before cursor", "bar", ic.getTextBeforeCursor(3, 0));

            // Test getTextAfterCursor
            fAssertEquals("Can retrieve text after cursor", "", ic.getTextAfterCursor(3, 0));
        }
    }
}
