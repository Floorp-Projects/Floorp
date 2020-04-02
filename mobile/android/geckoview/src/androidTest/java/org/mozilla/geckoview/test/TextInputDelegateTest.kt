/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.SystemClock
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import androidx.test.filters.MediumTest
import android.text.InputType;
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.ExtractedTextRequest
import android.view.inputmethod.InputConnection

import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.junit.runners.Parameterized.Parameter
import org.mozilla.geckoview.test.util.UiThreadUtils
import java.util.*
import java.util.concurrent.atomic.AtomicBoolean

@MediumTest
@RunWith(Parameterized::class)
class TextInputDelegateTest : BaseSessionTest() {
    // "parameters" needs to be a static field, so it has to be in a companion object.
    companion object {
        @get:Parameterized.Parameters(name = "{0}")
        @JvmStatic
        val parameters: List<Array<out Any>> = listOf(
                arrayOf("#input"),
                arrayOf("#textarea"),
                arrayOf("#contenteditable"),
                arrayOf("#designmode"))
    }

    @field:Parameter(0) @JvmField var id: String = ""

    private var textContent: String
        get() = when (id) {
            "#contenteditable" -> mainSession.evaluateJS("document.querySelector('$id').textContent")
            "#designmode" -> mainSession.evaluateJS("document.querySelector('$id').contentDocument.body.textContent")
            else -> mainSession.evaluateJS("document.querySelector('$id').value")
        } as String
        set(content) {
            when (id) {
                "#contenteditable" -> mainSession.evaluateJS("document.querySelector('$id').textContent = '$content'")
                "#designmode" -> mainSession.evaluateJS(
                        "document.querySelector('$id').contentDocument.body.textContent = '$content'")
                else -> mainSession.evaluateJS("document.querySelector('$id').value = '$content'")
            }
        }

    private val selectionOffsets: Pair<Int, Int> get() = when (id) {
        "#contenteditable" -> mainSession.evaluateJS("""[
                document.getSelection().anchorOffset,
                document.getSelection().focusOffset]""")
        "#designmode" -> mainSession.evaluateJS("""(function() {
                    var sel = document.querySelector('$id').contentDocument.getSelection();
                    var text = document.querySelector('$id').contentDocument.body.firstChild;
                    return [sel.anchorOffset, sel.focusOffset];
                })()""")
        else -> mainSession.evaluateJS("""(document.querySelector('$id').selectionDirection !== 'backward'
            ? [ document.querySelector('$id').selectionStart, document.querySelector('$id').selectionEnd ]
            : [ document.querySelector('$id').selectionEnd, document.querySelector('$id').selectionStart ])""")
    }.asJsonArray().let {
        Pair(it.getInt(0), it.getInt(1))
    }

    private fun processParentEvents() {
        sessionRule.requestedLocales
    }

    private fun processChildEvents() {
        mainSession.waitForJS("new Promise(r => window.setTimeout(r))")
    }

    private fun setComposingText(ic: InputConnection, text: CharSequence, newCursorPosition: Int) {
        val promise = mainSession.evaluatePromiseJS(
                when (id) {
                    "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('compositionupdate', r, { once: true }))"
                    else -> "new Promise(r => document.querySelector('$id').addEventListener('compositionupdate', r, { once: true }))"
                })
        ic.setComposingText(text, newCursorPosition)
        promise.value
    }

    private fun finishComposingText(ic: InputConnection) {
        val promise = mainSession.evaluatePromiseJS(
                when (id) {
                    "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('compositionend', r, { once: true }))"
                    else -> "new Promise(r => document.querySelector('$id').addEventListener('compositionend', r, { once: true }))"
                })
        ic.finishComposingText()
        promise.value
    }

    private fun pressKey(keyCode: Int) {
        // Create a Promise to listen to the key event, and wait on it below.
        val promise = mainSession.evaluatePromiseJS(
                "new Promise(r => window.addEventListener('keyup', r, { once: true }))")
        val time = SystemClock.uptimeMillis()
        val keyEvent = KeyEvent(time, time, KeyEvent.ACTION_DOWN, keyCode, 0)
        mainSession.textInput.onKeyDown(keyCode, keyEvent)
        mainSession.textInput.onKeyUp(keyCode, KeyEvent.changeAction(keyEvent, KeyEvent.ACTION_UP))
        promise.value
    }

    @Test fun restartInput() {
        // Check that restartInput is called on focus and blur.
        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }
        })

        mainSession.evaluateJS("document.querySelector('$id').blur()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_BLUR))
            }

            // Also check that showSoftInput/hideSoftInput are not called before a user action.
            @AssertCalled(count = 0)
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    @Test fun restartInput_temporaryFocus() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))
        // Disable for frequent failures Bug 1542525
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Focus the input once here and once below, but we should only get a
        // single restartInput or showSoftInput call for the second focus.
        mainSession.evaluateJS("document.querySelector('$id').focus(); document.querySelector('$id').blur()")

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("document.querySelector('$id').focus()")

        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
                super.showSoftInput(session)
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
                super.hideSoftInput(session)
            }
        })
    }

    @Test fun restartInput_temporaryBlur() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class,
                                    "restartInput", "showSoftInput")

        // We should get a pair of restartInput calls for the blur/focus,
        // but only one showSoftInput call and no hideSoftInput call.
        mainSession.evaluateJS("document.querySelector('$id').blur(); document.querySelector('$id').focus()")

        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 2, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct", reason, equalTo(forEachCall(
                        GeckoSession.TextInputDelegate.RESTART_REASON_BLUR,
                        GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS)))
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    @Test fun showHideSoftInput() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)

        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })

        mainSession.evaluateJS("document.querySelector('$id').blur()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
            }

            @AssertCalled(count = 0)
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 1, order = [2])
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    private fun getText(ic: InputConnection) =
            ic.getExtractedText(ExtractedTextRequest(), 0).text.toString()

    private fun assertText(message: String, actual: String, expected: String) =
            // In an HTML editor, Gecko may insert an additional element that show up as a
            // return character at the end. Deal with that here.
            assertThat(message, actual.trimEnd('\n'), equalTo(expected))

    private fun assertText(message: String, ic: InputConnection, expected: String,
                           checkGecko: Boolean = true) {
        processChildEvents()
        processParentEvents()

        if (checkGecko) {
            assertText(message, textContent, expected)
        }
        assertText(message, getText(ic), expected)
    }

    private fun assertSelection(message: String, ic: InputConnection, start: Int, end: Int,
                                checkGecko: Boolean = true) {
        processChildEvents()
        processParentEvents()

        if (checkGecko) {
            assertThat(message, selectionOffsets, equalTo(Pair(start, end)))
        }

        val extracted = ic.getExtractedText(ExtractedTextRequest(), 0)
        assertThat(message, extracted.selectionStart, equalTo(start))
        assertThat(message, extracted.selectionEnd, equalTo(end))
    }

    private fun assertSelectionAt(message: String, ic: InputConnection, value: Int,
                                  checkGecko: Boolean = true) =
            assertSelection(message, ic, value, value, checkGecko)

    private fun assertTextAndSelection(message: String, ic: InputConnection,
                                       expected: String, start: Int, end: Int,
                                       checkGecko: Boolean = true) {
        processChildEvents()
        processParentEvents()

        if (checkGecko) {
            assertText(message, textContent, expected)
            assertThat(message, selectionOffsets, equalTo(Pair(start, end)))
        }

        val extracted = ic.getExtractedText(ExtractedTextRequest(), 0)
        assertText(message, extracted.text.toString(), expected)
        assertThat(message, extracted.selectionStart, equalTo(start))
        assertThat(message, extracted.selectionEnd, equalTo(end))
    }

    private fun assertTextAndSelectionAt(message: String, ic: InputConnection,
                                         expected: String, value: Int,
                                         checkGecko: Boolean = true) =
            assertTextAndSelection(message, ic, expected, value, value, checkGecko)

    @WithDisplay(width = 512, height = 512) // Child process updates require having a display.
    @Test fun inputConnection() {
        // too slow on debug
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))
        // too flaky on x86
        assumeThat(sessionRule.env.isX86, equalTo(false))

        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = "foo"
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "foo")

        // Test setSelection
        ic.setSelection(0, 3)
        assertSelection("Can set selection to range", ic, 0, 3)
        ic.setSelection(-3, 6)
        // Test both forms of assert
        assertTextAndSelection("Can handle invalid range", ic,
                               "foo", 0, 3)
        ic.setSelection(3, 3)
        assertSelectionAt("Can collapse selection", ic, 3)
        ic.setSelection(4, 4)
        assertTextAndSelectionAt("Can handle invalid cursor", ic, "foo", 3)

        // Test commitText
        ic.commitText("", 10) // Selection past end of new text
        assertTextAndSelectionAt("Can commit empty text", ic, "foo", 3)
        ic.commitText("bar", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit text (select after)", ic,
                                 "foobar", 6)
        ic.commitText("foo", -1) // Selection at start of new text
        assertTextAndSelectionAt("Can commit text (select before)", ic,
                                 "foobarfoo", 5, /* checkGecko */ false)

        // Test deleteSurroundingText
        ic.deleteSurroundingText(1, 0)
        assertTextAndSelectionAt("Can delete text before", ic,
                                 "foobrfoo", 4)
        ic.deleteSurroundingText(1, 1)
        assertTextAndSelectionAt("Can delete text before/after", ic,
                                 "foofoo", 3)
        ic.deleteSurroundingText(0, 10)
        assertTextAndSelectionAt("Can delete text after", ic, "foo", 3)
        ic.deleteSurroundingText(0, 0)
        assertTextAndSelectionAt("Can delete empty text", ic, "foo", 3)

        // Test setComposingText
        setComposingText(ic, "foo", 1)
        assertTextAndSelectionAt("Can start composition", ic, "foofoo", 6)
        setComposingText(ic, "", 1)
        assertTextAndSelectionAt("Can set empty composition", ic, "foo", 3)
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt("Can update composition", ic, "foobar", 6)

        // Test finishComposingText
        ic.finishComposingText()
        assertTextAndSelectionAt("Can finish composition", ic, "foobar", 6)

        // Test setComposingRegion
        ic.setComposingRegion(0, 3)
        assertTextAndSelectionAt("Can set composing region", ic, "foobar", 6)

        setComposingText(ic, "far", 1)
        assertTextAndSelectionAt("Can set composing region text", ic,
                                 "farbar", 3)

        ic.setComposingRegion(1, 4)
        assertTextAndSelectionAt("Can set existing composing region", ic,
                                 "farbar", 3)

        setComposingText(ic, "rab", 3)
        assertTextAndSelectionAt("Can set new composing region text", ic,
                                 "frabar", 6, /* checkGecko */ false)

        // Test getTextBeforeCursor
        assertThat("Can retrieve text before cursor",
                   "bar", equalTo(ic.getTextBeforeCursor(3, 0)))

        // Test getTextAfterCursor
        assertThat("Can retrieve text after cursor",
                   "", equalTo(ic.getTextAfterCursor(3, 0)))

        ic.finishComposingText()
        assertTextAndSelectionAt("Can finish composition", ic,
                                 "frabar", 6, /* checkGecko */ false)

        // Test sendKeyEvent
        val time = SystemClock.uptimeMillis()
        val shiftKey = KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_SHIFT_LEFT, 0)
        val leftKey = KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                               KeyEvent.KEYCODE_DPAD_LEFT, 0)
        val tKey = KeyEvent(time, time, KeyEvent.ACTION_DOWN,
                            KeyEvent.KEYCODE_T, 0)

        ic.sendKeyEvent(shiftKey)
        ic.sendKeyEvent(leftKey)
        ic.sendKeyEvent(KeyEvent.changeAction(leftKey, KeyEvent.ACTION_UP))
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        assertTextAndSelection("Can select using key event", ic,
                               "frabar", 6, 5)

        ic.sendKeyEvent(tKey)
        ic.sendKeyEvent(KeyEvent.changeAction(tKey, KeyEvent.ACTION_UP))
        assertTextAndSelectionAt("Can type using event", ic, "frabat", 6)

        ic.deleteSurroundingText(6, 0)
        assertTextAndSelectionAt("Can clear text", ic, "", 0)

        // Bug 1133802, duplication when setting the same composing text more than once.
        setComposingText(ic, "foo", 1)
        assertTextAndSelectionAt("Can set the composing text", ic, "foo", 3)
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("foo", 1)
        assertTextAndSelectionAt("Can set the same composing text", ic,
                                 "foo", 3)
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt("Can set different composing text", ic,
                                 "bar", 3)
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("bar", 1)
        assertTextAndSelectionAt("Can set the same composing text", ic,
                                 "bar", 3)
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("bar", 1)
        assertTextAndSelectionAt("Can set the same composing text again", ic,
                                 "bar", 3)
        ic.finishComposingText()
        assertTextAndSelectionAt("Can finish composing text", ic, "bar", 3)

        ic.deleteSurroundingText(3, 0)
        assertTextAndSelectionAt("Can clear text", ic, "", 0)

        // Bug 1209465, cannot enter ideographic space character by itself (U+3000).
        ic.commitText("\u3000", 1)
        assertTextAndSelectionAt("Can commit ideographic space", ic,
                                 "\u3000", 1)

        ic.deleteSurroundingText(1, 0)
        assertTextAndSelectionAt("Can clear text", ic, "", 0)

        // Bug 1275371 - shift+backspace should not forward delete on Android.
        val delKey = KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0)

        ic.beginBatchEdit()
        ic.commitText("foo", 1)
        ic.setSelection(1, 1)
        ic.endBatchEdit()
        assertTextAndSelectionAt("Can commit text", ic, "foo", 1)

        ic.sendKeyEvent(shiftKey)
        ic.sendKeyEvent(delKey)
        ic.sendKeyEvent(KeyEvent.changeAction(delKey, KeyEvent.ACTION_UP))
        assertTextAndSelectionAt("Can backspace with shift+backspace", ic,
                                 "oo", 0)

        ic.sendKeyEvent(delKey)
        ic.sendKeyEvent(KeyEvent.changeAction(delKey, KeyEvent.ACTION_UP))
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        assertTextAndSelectionAt("Cannot forward delete with shift+backspace", ic,
                                 "oo", 0)

        ic.deleteSurroundingText(0, 2)
        assertTextAndSelectionAt("Can clear text", ic, "", 0)

        // Bug 1490391 - Committing then setting composition can result in duplicates.
        ic.commitText("far", 1)
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt("Can commit then set composition", ic,
                                 "farbar", 6)
        setComposingText(ic, "baz", 1)
        assertTextAndSelectionAt("Composition still exists after setting", ic,
                                 "farbaz", 6)

        ic.finishComposingText()
        ic.deleteSurroundingText(6, 0)
        assertTextAndSelectionAt("Can clear text", ic, "", 0)
    }

    @WithDisplay(width = 512, height = 512) // Child process updates require having a display.
    @Test fun sendDummpyKeyboardEvent() {
        // unnecessary for designmode
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        ic.commitText("a", 1)

        // Dispatching keydown, input and keyup
        val promise =
            mainSession.evaluatePromiseJS("""
                new Promise(r => window.addEventListener('keydown', () => {
                                     window.addEventListener('input',() => {
                                         window.addEventListener('keyup', r, { once: true }) },
                                         { once: true }) },
                                     { once: true}))""")
        ic.beginBatchEdit();
        ic.setSelection(0, 1)
        ic.setComposingText("", 1)
        ic.endBatchEdit()
        promise.value
        assertText("empty text", ic, "")
    }

    @WithDisplay(width = 512, height = 512) // Child process updates require having a display.
    @Test fun editorInfo_default() {
        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val editorInfo = EditorInfo()
        mainSession.textInput.onCreateInputConnection(editorInfo)
        assertThat("Default EditorInfo.inputType", editorInfo.inputType, equalTo(
            when (id) {
                "#input" -> InputType.TYPE_CLASS_TEXT or
                            InputType.TYPE_TEXT_FLAG_AUTO_CORRECT or
                            InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE
                else -> InputType.TYPE_CLASS_TEXT or
                        InputType.TYPE_TEXT_FLAG_CAP_SENTENCES or
                        InputType.TYPE_TEXT_FLAG_AUTO_CORRECT or
                        InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE
            }))
    }

    @WithDisplay(width = 512, height = 512) // Child process updates require having a display.
    @Test fun bug1613804_finishComposingText() {
        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        ic.beginBatchEdit();
        ic.setComposingText("abc", 1)
        ic.endBatchEdit()

        // finishComposingText has to dispatch compositionend event.
        finishComposingText(ic)

        assertText("commit abc", ic, "abc")
    }
}
