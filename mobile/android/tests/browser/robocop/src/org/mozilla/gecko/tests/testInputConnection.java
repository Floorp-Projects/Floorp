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

import com.robotium.solo.Condition;

import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Tests the proper operation of GeckoInputConnection
 */
public class testInputConnection extends JavascriptBridgeTest {

    private static final String INITIAL_TEXT = "foo";

    public void testInputConnection() throws InterruptedException {
        GeckoHelper.blockForReady();

        final String url = mStringHelper.ROBOCOP_INPUT_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        // First run tests inside the normal input field.
        getJS().syncCall("focus_input", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest());

        // Then switch focus to the text area and rerun tests.
        getJS().syncCall("focus_text_area", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest());

        // Then switch focus to the content editable and rerun tests.
        getJS().syncCall("focus_content_editable", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest());

        // Then switch focus to the design mode document and rerun tests.
        getJS().syncCall("focus_design_mode", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest());

        // Then switch focus to the resetting input field, and run tests there.
        getJS().syncCall("focus_resetting_input", "");
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new ResettingInputConnectionTest());

        // Then switch focus to the hiding input field, and run tests there.
        getJS().syncCall("focus_hiding_input", "");
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new HidingInputConnectionTest());

        getJS().syncCall("finish_test");
    }

    private class BasicInputConnectionTest extends InputConnectionTest {
        @Override
        public void test(final InputConnection ic, EditorInfo info) {
            waitFor("focus change", new Condition() {
                @Override
                public boolean isSatisfied() {
                    return INITIAL_TEXT.equals(getText(ic));
                }
            });

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

            // Bug 1051556, exception due to committing text changes during flushing.
            ic.setComposingText("bad", 1);
            assertTextAndSelectionAt("Can set the composing text", ic, "bad", 3);
            getJS().asyncCall("test_reflush_changes");
            // Wait for text change notifications to come in.
            processGeckoEvents(ic);
            assertTextAndSelectionAt("Can re-flush text changes", ic, "good", 4);
            ic.setComposingText("done", 1);
            assertTextAndSelectionAt("Can update composition after re-flushing", ic, "done", 4);
            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composing text", ic, "done", 4);

            ic.deleteSurroundingText(4, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1241558 - wrong selection due to ignoring selection notification.
            ic.setComposingText("foobar", 1);
            assertTextAndSelectionAt("Can set the composing text", ic, "foobar", 6);
            getJS().asyncCall("test_set_selection");
            // Wait for text change notifications to come in.
            processGeckoEvents(ic);
            assertTextAndSelectionAt("Can select after committing", ic, "foobar", 3);
            ic.setComposingText("barfoo", 1);
            assertTextAndSelectionAt("Can compose after selecting", ic, "barfoo", 6);
            ic.beginBatchEdit();
            ic.setSelection(3, 3);
            ic.finishComposingText();
            ic.deleteSurroundingText(1, 1);
            ic.endBatchEdit();
            assertTextAndSelectionAt("Can delete after committing", ic, "baoo", 2);

            ic.deleteSurroundingText(2, 2);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1275371 - shift+backspace should not forward delete on Android.
            final KeyEvent delKey = new KeyEvent(KeyEvent.ACTION_DOWN,
                                                 KeyEvent.KEYCODE_DEL);

            ic.beginBatchEdit();
            ic.commitText("foo", 1);
            ic.setSelection(1, 1);
            ic.endBatchEdit();
            assertTextAndSelectionAt("Can commit text", ic, "foo", 1);

            ic.sendKeyEvent(shiftKey);
            ic.sendKeyEvent(delKey);
            ic.sendKeyEvent(KeyEvent.changeAction(delKey, KeyEvent.ACTION_UP));
            assertTextAndSelectionAt("Can backspace with shift+backspace", ic, "oo", 0);

            ic.sendKeyEvent(delKey);
            ic.sendKeyEvent(KeyEvent.changeAction(delKey, KeyEvent.ACTION_UP));
            ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP));
            assertTextAndSelectionAt("Cannot forward delete with shift+backspace", ic, "oo", 0);

            ic.deleteSurroundingText(0, 2);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents(ic);
            processInputConnectionEvents();
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

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents(ic);
            processInputConnectionEvents();
        }
    }

    /**
     * HidingInputConnectionTest performs tests on the hiding input in
     * robocop_input.html. Any test that uses the normal input should be put in
     * BasicInputConnectionTest.
     */
    private class HidingInputConnectionTest extends InputConnectionTest {
        @Override
        public void test(final InputConnection ic, EditorInfo info) {
            waitFor("focus change", new Condition() {
                @Override
                public boolean isSatisfied() {
                    return "".equals(getText(ic));
                }
            });

            // Bug 1254629, crash when hiding input during input.
            ic.commitText("foo", 1);
            assertTextAndSelectionAt("Can commit text (hiding)", ic, "foo", 3);

            ic.commitText("!", 1);
            // The '!' key causes the input to hide in robocop_input.html,
            // and there won't be a text/selection update as a result.
            assertTextAndSelectionAt("Can handle hiding input", ic, "foo", 3);

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents(ic);
            processInputConnectionEvents();
        }
    }
}
