/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertSelection;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertSelectionAt;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertText;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertTextAndSelection;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.assertTextAndSelectionAt;
import static org.mozilla.gecko.tests.helpers.TextInputHelper.getText;
import static org.mozilla.gecko.tests.helpers.WaitHelper.waitFor;

import org.mozilla.gecko.tests.components.GeckoViewComponent.InputConnectionTest;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import com.jayway.android.robotium.solo.Condition;

import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Tests the proper operation of GeckoInputConnection
 */
public class testInputConnection extends UITest {

    private static final String INITIAL_TEXT = "foo";

    public void testInputConnection() throws InterruptedException {
        GeckoHelper.blockForReady();

        final String url = mStringHelper.ROBOCOP_INPUT_URL + "#" + INITIAL_TEXT;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        mGeckoView.mTextInput
            .waitForInputConnection()
            // First run tests inside the normal input field.
            .testInputConnection(new BasicInputConnectionTest())
            // Then switch focus to the resetting input field, and run tests there.
            .testInputConnection(new FocusNextInputFieldTest())
            .testInputConnection(new ResettingInputConnectionTest());
    }

    private class BasicInputConnectionTest extends InputConnectionTest {
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

            // Test setComposingRegion
            ic.setComposingRegion(0, 3);
            assertTextAndSelectionAt("Can set composing region", ic, "foobar", 6);

            ic.setComposingText("far", 1);
            assertTextAndSelectionAt("Can set composing region text", ic, "farbar", 3);

            ic.setComposingRegion(1, 4);
            assertTextAndSelectionAt("Can set existing composing region", ic, "farbar", 3);

            ic.setComposingText("rab", 3);
            assertTextAndSelectionAt("Can set new composing region text", ic, "frabar", 6);

            // Test getTextBeforeCursor
            fAssertEquals("Can retrieve text before cursor", "bar", ic.getTextBeforeCursor(3, 0));

            // Test getTextAfterCursor
            fAssertEquals("Can retrieve text after cursor", "", ic.getTextAfterCursor(3, 0));

            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composition", ic, "frabar", 6);

            // Test sendKeyEvent
            final KeyEvent shiftKey = new KeyEvent(KeyEvent.ACTION_DOWN,
                                                   KeyEvent.KEYCODE_SHIFT_LEFT);
            final KeyEvent leftKey = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT);
            final KeyEvent tKey = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T);

            ic.sendKeyEvent(shiftKey);
            ic.sendKeyEvent(leftKey);
            ic.sendKeyEvent(KeyEvent.changeAction(leftKey, KeyEvent.ACTION_UP));
            ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP));
            assertTextAndSelection("Can select using key event", ic, "frabar", 6, 5);

            ic.sendKeyEvent(tKey);
            ic.sendKeyEvent(KeyEvent.changeAction(tKey, KeyEvent.ACTION_UP));
            assertTextAndSelectionAt("Can type using event", ic, "frabat", 6);

            ic.deleteSurroundingText(6, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1133802, duplication when setting the same composing text more than once.
            ic.setComposingText("foo", 1);
            assertTextAndSelectionAt("Can set the composing text", ic, "foo", 3);
            ic.setComposingText("foo", 1);
            assertTextAndSelectionAt("Can set the same composing text", ic, "foo", 3);
            ic.setComposingText("bar", 1);
            assertTextAndSelectionAt("Can set different composing text", ic, "bar", 3);
            ic.setComposingText("bar", 1);
            assertTextAndSelectionAt("Can set the same composing text", ic, "bar", 3);
            ic.setComposingText("bar", 1);
            assertTextAndSelectionAt("Can set the same composing text again", ic, "bar", 3);
            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composing text", ic, "bar", 3);

            ic.deleteSurroundingText(3, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1209465, cannot enter ideographic space character by itself (U+3000).
            ic.commitText("\u3000", 1);
            assertTextAndSelectionAt("Can commit ideographic space", ic, "\u3000", 1);

            ic.deleteSurroundingText(1, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);
        }
    }

    /**
     * FocusNextInputFieldTest is used to switch focus from one input field to
     * another on the test page by sending a tab key.
     */
    private class FocusNextInputFieldTest extends InputConnectionTest {
        @Override
        public void test(final InputConnection ic, EditorInfo info) {
            // First clear all text.
            ic.setSelection(0, 0);
            assertSelectionAt("Can set selection to start", ic, 0);

            ic.deleteSurroundingText(0, Integer.MAX_VALUE);
            assertTextAndSelectionAt("Can clear all text", ic, "", 0);

            // Set dummy text in currently focused input so we can check when we have switched focus.
            final String dummyText = "dummy switch input text";
            ic.commitText(dummyText, 1);
            assertTextAndSelectionAt("Can commit text", ic, dummyText, dummyText.length());

            // Finish processing events from the old input field.
            processGeckoEvents(ic);
            processInputConnectionEvents();

            final KeyEvent tabKey = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
            ic.sendKeyEvent(tabKey);
            ic.sendKeyEvent(KeyEvent.changeAction(tabKey, KeyEvent.ACTION_UP));
        }
    }

    /**
     * ResettingInputConnectionTest performs tests on the resetting input in
     * robocop_input.html. Any test that uses the normal input should be put in
     * BasicInputConnectionTest.
     */
    private class ResettingInputConnectionTest extends InputConnectionTest {
        @Override
        public void test(final InputConnection ic, EditorInfo info) {
            waitFor("focus change", new Condition() {
                @Override
                public boolean isSatisfied() {
                    return "".equals(getText(ic));
                }
            });

            // Bug 1199658, duplication when page has JS that resets input field value.

            ic.commitText("foo", 1);
            assertTextAndSelectionAt("Can commit text (resetting)", ic, "foo", 3);

            ic.setComposingRegion(0, 3);
            // The bug appears after composition update events are processed. We only
            // issue these events after some back-and-forth calls between the Gecko thread
            // and the input connection thread. Therefore, to ensure these events are
            // issued and to ensure the bug appears, we have to process all Gecko events,
            // then all input connection events, and finally all Gecko events again.
            processGeckoEvents(ic);
            processInputConnectionEvents();
            processGeckoEvents(ic);
            assertTextAndSelectionAt("Can set composing region (resetting)", ic, "foo", 3);

            ic.setComposingText("foobar", 1);
            processGeckoEvents(ic);
            processInputConnectionEvents();
            processGeckoEvents(ic);
            assertTextAndSelectionAt("Can change composing text (resetting)", ic, "foobar", 6);

            ic.setComposingText("baz", 1);
            processGeckoEvents(ic);
            processInputConnectionEvents();
            processGeckoEvents(ic);
            assertTextAndSelectionAt("Can reset composing text (resetting)", ic, "baz", 3);

            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composing text (resetting)", ic, "baz", 3);

            ic.deleteSurroundingText(3, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);
        }
    }
}
