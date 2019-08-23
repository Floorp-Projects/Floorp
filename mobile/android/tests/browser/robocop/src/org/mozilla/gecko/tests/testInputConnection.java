/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.WaitHelper.waitFor;

import org.mozilla.gecko.tests.components.GeckoViewComponent.InputConnectionTest;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import com.robotium.solo.Condition;

import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Tests the proper operation of GeckoInputConnection
 */
public class testInputConnection extends JavascriptBridgeTest {

    private static final String INITIAL_TEXT = "foo";

    private String mEventsLog;
    private String mKeyLog;

    public void testInputConnection() throws InterruptedException {
        GeckoHelper.blockForReady();

        // Spatial navigation interferes with design-mode key event tests.
        mActions.setPref("snav.enabled", false, /* flush */ false);
        // Enable "selectionchange" events for input/textarea.
        mActions.setPref("dom.select_events.enabled", true, /* flush */ false);
        mActions.setPref("dom.select_events.textcontrols.enabled", true, /* flush */ false);
        // Enable dummy key synthesis.
        mActions.setPref("intl.ime.hack.on_any_apps.fire_key_events_for_composition",
                         true, /* flush */ false);

        final String url = mStringHelper.ROBOCOP_INPUT_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        // First run tests inside the normal input field.
        getJS().syncCall("focus_input", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest("input"));

        // Then switch focus to the text area and rerun tests.
        getJS().syncCall("focus_text_area", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest("textarea"));

        // Then switch focus to the content editable and rerun tests.
        getJS().syncCall("focus_content_editable", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest("contentEditable"));

        // Then switch focus to the design mode document and rerun tests.
        getJS().syncCall("focus_design_mode", INITIAL_TEXT);
        mGeckoView.mTextInput
            .waitForInputConnection()
            .testInputConnection(new BasicInputConnectionTest("designMode"));

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

    public void setEventsLog(final String log) {
        mEventsLog = log;
    }

    public String getEventsLog() {
        return mEventsLog;
    }

    public void setKeyLog(final String log) {
        mKeyLog = log;
    }

    public String getKeyLog() {
        return mKeyLog;
    }

    private class BasicInputConnectionTest extends InputConnectionTest {
        private final String mType;

        BasicInputConnectionTest(final String type) {
            mType = type;
        }

        private void pressKey(final InputConnection ic, int keycode) {
            final long time = SystemClock.uptimeMillis();
            final KeyEvent key = new KeyEvent(time, time, KeyEvent.ACTION_DOWN, keycode, 0);

            ic.sendKeyEvent(key);
            ic.sendKeyEvent(KeyEvent.changeAction(key, KeyEvent.ACTION_UP));
        }

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

            ic.deleteSurroundingText(6, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Test sendKeyEvent
            pressKey(ic, KeyEvent.KEYCODE_F);
            pressKey(ic, KeyEvent.KEYCODE_R);
            pressKey(ic, KeyEvent.KEYCODE_A);
            pressKey(ic, KeyEvent.KEYCODE_B);
            pressKey(ic, KeyEvent.KEYCODE_A);
            pressKey(ic, KeyEvent.KEYCODE_R);
            assertTextAndSelectionAt("Can input text by keyboard", ic, "frabar", 6);

            final long time = SystemClock.uptimeMillis();
            final KeyEvent shiftKey = new KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                                                   KeyEvent.KEYCODE_SHIFT_LEFT, 0);
            final KeyEvent leftKey = new KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                                                  KeyEvent.KEYCODE_DPAD_LEFT, 0);

            ic.sendKeyEvent(shiftKey);
            ic.sendKeyEvent(leftKey);
            ic.sendKeyEvent(KeyEvent.changeAction(leftKey, KeyEvent.ACTION_UP));
            ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP));
            assertTextAndSelection("Can select using key event", ic, "frabar", 6, 5);

            pressKey(ic, KeyEvent.KEYCODE_T);
            assertTextAndSelectionAt("Can type using event", ic, "frabat", 6);

            ic.deleteSurroundingText(6, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Test key synthesis.
            getJS().syncCall("start_key_log");
            ic.setComposingText("f", 1); // Synthesizes dummy key.
            assertTextAndSelectionAt("Can compose F key", ic, "f", 1);
            ic.finishComposingText(); // Does not synthesize key.
            assertTextAndSelectionAt("Can finish F key", ic, "f", 1);
            ic.commitText("o", 1); // Synthesizes O key.
            assertTextAndSelectionAt("Can commit O key", ic, "fo", 2);
            ic.commitText("of", 1); // Synthesizes dummy key.
            assertTextAndSelectionAt("Can commit non-key string", ic, "foof", 4);

            getJS().syncCall("end_key_log");
            fAssertEquals("Can synthesize keys",
                          "keydown:Process,casm;keyup:Process,casm;" +     // Dummy
                          "keydown:o,casm;keypress:o,casm;keyup:o,casm;" + // O key
                          "keydown:Process,casm;keyup:Process,casm;",      // Dummy
                          getKeyLog());

            ic.deleteSurroundingText(4, 0);
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
            processGeckoEvents();
            assertTextAndSelectionAt("Can re-flush text changes", ic, "good", 4);
            ic.setComposingText("done", 1);
            assertTextAndSelectionAt("Can update composition after re-flushing", ic, "gooddone", 8);
            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composing text", ic, "gooddone", 8);

            ic.deleteSurroundingText(8, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1241558 - wrong selection due to ignoring selection notification.
            ic.setComposingText("foobar", 1);
            assertTextAndSelectionAt("Can set the composing text", ic, "foobar", 6);
            getJS().asyncCall("test_set_selection");
            // Wait for text change notifications to come in.
            processGeckoEvents();
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
            final KeyEvent delKey = new KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                                                 KeyEvent.KEYCODE_DEL, 0);

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

            // Bug 1123514 - exception due to incorrect text replacement offsets.
            getJS().syncCall("test_bug1123514");
            // Gecko will change text to 'abc' when we input 'b', potentially causing
            // incorrect calculation of text replacement offsets.
            ic.commitText("b", 1);
            // This test only works for input/textarea,
            if (mType.equals("input") || mType.equals("textarea")) {
                assertTextAndSelectionAt("Can handle text replacement", ic, "abc", 3);
            } else {
                processGeckoEvents();
                processInputConnectionEvents();
            }

            ic.deleteSurroundingText(3, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1307816 - Don't end then start composition when setting
            // composing text, which can confuse the Facebook comment box.
            getJS().syncCall("start_events_log");
            ic.setComposingText("f", 1);
            processGeckoEvents();
            ic.setComposingText("fo", 1);
            processGeckoEvents();
            ic.setComposingText("foo", 1);
            processGeckoEvents();
            ic.finishComposingText();
            assertTextAndSelectionAt("Can reuse composition in Java", ic, "foo", 3);

            getJS().syncCall("end_events_log");
            if (mType.equals("textarea")) {
                // textarea has a buggy selectionchange behavior.
                fAssertEquals("Can reuse composition in Gecko", "<=|==", getEventsLog());
            } else {
                // compositionstart > (compositionchange > selectionchange) x3
                fAssertEquals("Can reuse composition in Gecko", "<=|=|=|", getEventsLog());
            }

            ic.deleteSurroundingText(3, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1353799 - Can set selection while having a composition, so
            // the caret can be moved to the start of the composition.
            getJS().syncCall("start_events_log");
            ic.setComposingText("foo", 1);
            assertTextAndSelectionAt("Can set composition before selection", ic, "foo", 3);
            ic.setSelection(0, 0);
            assertTextAndSelectionAt("Can set selection after composition", ic, "foo", 0);

            getJS().syncCall("end_events_log");
            // compositionstart > compositionchange > selectionchange x2
            fAssertEquals("Can update composition caret", "<=||", getEventsLog());

            ic.finishComposingText();
            ic.deleteSurroundingText(0, 3);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1387889 - Latin sharp S (U+00DF) triggers Alt+S shortcut
            getJS().syncCall("start_key_log");
            ic.commitText("\u00df", 1); // Synthesizes "Latin sharp S" key without modifiers.
            assertTextAndSelectionAt("Can commit Latin sharp S key", ic, "\u00df", 1);

            getJS().syncCall("end_key_log");
            fAssertEquals("Can synthesize sharp S key",
                          "keydown:\u00df,casm;keypress:\u00df,casm;keyup:\u00df,casm;",
                          getKeyLog());

            ic.finishComposingText();
            ic.deleteSurroundingText(1, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Bug 1490391 - Committing then setting composition can result in duplicates.
            ic.commitText("far", 1);
            ic.setComposingText("bar", 1);
            assertTextAndSelectionAt("Can commit then set composition", ic, "farbar", 6);
            ic.setComposingText("baz", 1);
            assertTextAndSelectionAt("Composition still exists after setting", ic, "farbaz", 6);

            ic.finishComposingText();
            ic.deleteSurroundingText(6, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents();
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
            processGeckoEvents();
            processInputConnectionEvents();
            processGeckoEvents();
            assertTextAndSelectionAt("Can set composing region (resetting)", ic, "foo", 3);

            ic.setComposingText("foobar", 1);
            processGeckoEvents();
            processInputConnectionEvents();
            processGeckoEvents();
            assertTextAndSelectionAt("Can change composing text (resetting)", ic, "foobar", 6);

            ic.setComposingText("baz", 1);
            processGeckoEvents();
            processInputConnectionEvents();
            processGeckoEvents();
            assertTextAndSelectionAt("Can reset composing text (resetting)", ic, "baz", 3);

            ic.finishComposingText();
            assertTextAndSelectionAt("Can finish composing text (resetting)", ic, "baz", 3);

            ic.deleteSurroundingText(3, 0);
            assertTextAndSelectionAt("Can clear text", ic, "", 0);

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents();
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
            assertTextAndSelectionAt("Can handle hiding input", ic, "foo", 3);

            // Bug 1401737, Editable does not behave correctly after disconnecting from Gecko.
            getJS().syncCall("blur_hiding_input");
            processGeckoEvents();
            processInputConnectionEvents();

            ic.setComposingRegion(0, 3);
            ic.commitText("bar", 1);
            assertTextAndSelectionAt("Can set spans/text after blur", ic, "bar", 3);

            ic.commitText("baz", 1);
            assertTextAndSelectionAt("Can remove spans after blur", ic, "barbaz", 6);

            ic.setSelection(0, 3);
            assertTextAndSelection("Can set selection after blur", ic, "barbaz", 0, 3);

            // Make sure we don't leave behind stale events for the following test.
            processGeckoEvents();
            processInputConnectionEvents();
        }
    }
}
