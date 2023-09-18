/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.content.ClipDescription
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.InputType
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.ExtractedTextRequest
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputContentInfo
import androidx.test.filters.MediumTest
import androidx.test.filters.SdkSuppress
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.junit.runners.Parameterized.Parameter
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.TextInputDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

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
            arrayOf("#designmode"),
        )
    }

    @field:Parameter(0)
    @JvmField
    var id: String = ""

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
                    "document.querySelector('$id').contentDocument.body.textContent = '$content'",
                )
                else -> mainSession.evaluateJS("document.querySelector('$id').value = '$content'")
            }
        }

    private var selectionOffsets: Pair<Int, Int>
        get() = when (id) {
            "#contenteditable" -> mainSession.evaluateJS(
                """[
                    document.getSelection().anchorOffset,
                    document.getSelection().focusOffset]""",
            )
            "#designmode" -> mainSession.evaluateJS(
                """(function() {
                        var sel = document.querySelector('$id').contentDocument.getSelection();
                        var text = document.querySelector('$id').contentDocument.body.firstChild;
                        return [sel.anchorOffset, sel.focusOffset];
                    })()""",
            )
            else -> mainSession.evaluateJS(
                """(document.querySelector('$id').selectionDirection !== 'backward'
                ? [ document.querySelector('$id').selectionStart, document.querySelector('$id').selectionEnd ]
                : [ document.querySelector('$id').selectionEnd, document.querySelector('$id').selectionStart ])""",
            )
        }.asJsonArray().let {
            Pair(it.getInt(0), it.getInt(1))
        }
        set(offsets) {
            var (start, end) = offsets
            when (id) {
                "#contenteditable" -> mainSession.evaluateJS(
                    """(function() {
                        let selection = document.getSelection();
                        let text = document.querySelector('$id').firstChild;
                        if (text) {
                            selection.setBaseAndExtent(text, $start, text, $end)
                        } else {
                            selection.collapse(document.querySelector('$id'), 0);
                        }
                    })()""",
                )
                "#designmode" -> mainSession.evaluateJS(
                    """(function() {
                        let selection = document.querySelector('$id').contentDocument.getSelection();
                        let text = document.querySelector('$id').contentDocument.body.firstChild;
                        if (text) {
                            selection.setBaseAndExtent(text, $start, text, $end)
                        } else {
                            selection.collapse(document.querySelector('$id').contentDocument.body, 0);
                        }
                    })()""",
                )
                else -> mainSession.evaluateJS("document.querySelector('$id').setSelectionRange($start, $end)")
            }
        }

    private fun processParentEvents() {
        sessionRule.requestedLocales
    }

    private fun processChildEvents() {
        mainSession.waitForJS("new Promise(r => requestAnimationFrame(r))")
    }

    private fun setComposingText(ic: InputConnection, text: CharSequence, newCursorPosition: Int) {
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('compositionupdate', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('compositionupdate', r, { once: true }))"
            },
        )
        ic.setComposingText(text, newCursorPosition)
        promise.value
    }

    private fun finishComposingText(ic: InputConnection) {
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('compositionend', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('compositionend', r, { once: true }))"
            },
        )
        ic.finishComposingText()
        promise.value
    }

    private fun commitText(ic: InputConnection, text: CharSequence, newCursorPosition: Int) {
        if (text == "") {
            // No composition event is fired
            ic.commitText(text, newCursorPosition)
            return
        }
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('compositionend', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('compositionend', r, { once: true }))"
            },
        )
        ic.commitText(text, newCursorPosition)
        promise.value
    }

    private fun deleteSurroundingText(ic: InputConnection, before: Int, after: Int) {
        // deleteSurroundingText might fire multiple events.
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('input', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('input', r, { once: true }))"
            },
        )
        ic.deleteSurroundingText(before, after)
        if (before != 0 || after != 0) {
            promise.value
        }
        // XXX: No way to wait for all events.
        processChildEvents()
    }

    private fun setSelection(ic: InputConnection, start: Int, end: Int) {
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('selectionchange', r, { once: true }))"
                "#contenteditable" -> "new Promise(r => document.addEventListener('selectionchange', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('selectionchange', r, { once: true }))"
            },
        )
        ic.setSelection(start, end)
        promise.value
    }

    private fun pressKey(ic: InputConnection, keyCode: Int) {
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('keyup', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('keyup', r, { once: true }))"
            },
        )
        val time = SystemClock.uptimeMillis()
        val keyEvent = KeyEvent(time, time, KeyEvent.ACTION_DOWN, keyCode, 0)
        ic.sendKeyEvent(keyEvent)
        ic.sendKeyEvent(KeyEvent.changeAction(keyEvent, KeyEvent.ACTION_UP))
        promise.value
    }

    private fun syncShadowText(ic: InputConnection) {
        // Workaround for sync shadow text
        ic.beginBatchEdit()
        ic.endBatchEdit()
    }

    @Test fun restartInput() {
        // Check that restartInput is called on focus and blur.
        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(object : TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat(
                    "Reason should be correct",
                    reason,
                    equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS),
                )
            }
        })

        mainSession.evaluateJS("document.querySelector('$id').blur()")
        mainSession.waitUntilCalled(object : TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat(
                    "Reason should be correct",
                    reason,
                    equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_BLUR),
                )
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
        mainSession.pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("document.querySelector('$id').focus()")

        mainSession.waitUntilCalled(object : TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat(
                    "Reason should be correct",
                    reason,
                    equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS),
                )
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    @Test fun restartInput_temporaryBlur() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Simulate a user action so we're allowed to show/hide the keyboard.
        mainSession.pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(
            GeckoSession.TextInputDelegate::class,
            "restartInput",
            "showSoftInput",
        )

        // We should get a pair of restartInput calls for the blur/focus,
        // but only one showSoftInput call and no hideSoftInput call.
        mainSession.evaluateJS("document.querySelector('$id').blur(); document.querySelector('$id').focus()")

        mainSession.waitUntilCalled(object : TextInputDelegate {
            @AssertCalled(count = 2, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat(
                    "Reason should be correct",
                    reason,
                    equalTo(
                        forEachCall(
                            GeckoSession.TextInputDelegate.RESTART_REASON_BLUR,
                            GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS,
                        ),
                    ),
                )
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
        mainSession.pressKey(KeyEvent.KEYCODE_CTRL_LEFT)

        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(object : TextInputDelegate {
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
        mainSession.waitUntilCalled(object : TextInputDelegate {
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

    private fun assertText(
        message: String,
        ic: InputConnection,
        expected: String,
        checkGecko: Boolean = true,
    ) {
        processChildEvents()
        processParentEvents()

        if (checkGecko) {
            assertText(message, textContent, expected)
        }
        assertText(message, getText(ic), expected)
    }

    private fun assertSelection(
        message: String,
        ic: InputConnection,
        start: Int,
        end: Int,
        checkGecko: Boolean = true,
    ) {
        processChildEvents()
        processParentEvents()

        if (checkGecko) {
            assertThat(message, selectionOffsets, equalTo(Pair(start, end)))
        }

        val extracted = ic.getExtractedText(ExtractedTextRequest(), 0)
        assertThat(message, extracted.selectionStart, equalTo(start))
        assertThat(message, extracted.selectionEnd, equalTo(end))
    }

    private fun assertSelectionAt(
        message: String,
        ic: InputConnection,
        value: Int,
        checkGecko: Boolean = true,
    ) =
        assertSelection(message, ic, value, value, checkGecko)

    private fun assertTextAndSelection(
        message: String,
        ic: InputConnection,
        expected: String,
        start: Int,
        end: Int,
        checkGecko: Boolean = true,
    ) {
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

    private fun assertTextAndSelectionAt(
        message: String,
        ic: InputConnection,
        expected: String,
        value: Int,
        checkGecko: Boolean = true,
    ) =
        assertTextAndSelection(message, ic, expected, value, value, checkGecko)

    private fun setupContent(content: String) {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.select_events.textcontrols.enabled" to true,
            ),
        )

        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = content
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")
    }

    // Test setSelection
    @Ignore
    // Disable for frequent timeout for selection event.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_setSelection() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        // TODO:
        // onselectionchange won't be fired if caret is last. But commitText
        // can set text and selection well (Bug 1360388).
        commitText(ic, "foo", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit text", ic, "foo", 3)

        setSelection(ic, 0, 3)
        assertSelection("Can set selection to range", ic, 0, 3)
        // No selection change event is fired
        ic.setSelection(-3, 6)
        // Test both forms of assert
        assertTextAndSelection(
            "Can handle invalid range",
            ic,
            "foo",
            0,
            3,
        )
        setSelection(ic, 3, 3)
        assertSelectionAt("Can collapse selection", ic, 3)
        // No selection change event is fired
        ic.setSelection(4, 4)
        assertTextAndSelectionAt("Can handle invalid cursor", ic, "foo", 3)
    }

    // Test commitText
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_commitText() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "foo", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit empty text", ic, "foo", 3)

        commitText(ic, "", 10) // Selection past end of new text
        assertTextAndSelectionAt("Can commit empty text", ic, "foo", 3)
        commitText(ic, "bar", 1) // Selection at end of new text
        assertTextAndSelectionAt(
            "Can commit text (select after)",
            ic,
            "foobar",
            6,
        )
        commitText(ic, "foo", -1) // Selection at start of new text
        assertTextAndSelectionAt(
            "Can commit text (select before)",
            ic,
            "foobarfoo",
            5, /* checkGecko */
            false,
        )
    }

    // Test deleteSurroundingText
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_deleteSurroundingText() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        commitText(ic, "foobarfoo", 1)
        assertTextAndSelectionAt("Set initial text and selection", ic, "foobarfoo", 9)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        assertSelection("Can set selection to range", ic, 5, 5)

        deleteSurroundingText(ic, 1, 0)
        assertTextAndSelectionAt(
            "Can delete text before",
            ic,
            "foobrfoo",
            4,
        )
        deleteSurroundingText(ic, 1, 1)
        assertTextAndSelectionAt(
            "Can delete text before/after",
            ic,
            "foofoo",
            3,
        )
        deleteSurroundingText(ic, 0, 10)
        assertTextAndSelectionAt("Can delete text after", ic, "foo", 3)
        deleteSurroundingText(ic, 0, 0)
        assertTextAndSelectionAt("Can delete empty text", ic, "foo", 3)
    }

    // Test setComposingText
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_setComposingText() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "foo", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit text", ic, "foo", 3)

        setComposingText(ic, "foo", 1)
        assertTextAndSelectionAt("Can start composition", ic, "foofoo", 6)
        setComposingText(ic, "", 1)
        assertTextAndSelectionAt("Can set empty composition", ic, "foo", 3)
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt("Can update composition", ic, "foobar", 6)

        // Test finishComposingText
        finishComposingText(ic)
        assertTextAndSelectionAt("Can finish composition", ic, "foobar", 6)
    }

    // Test setComposingRegion
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_setComposingRegion() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "foobar", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit text", ic, "foobar", 6)

        ic.setComposingRegion(0, 3)
        assertTextAndSelectionAt("Can set composing region", ic, "foobar", 6)

        setComposingText(ic, "far", 1)
        assertTextAndSelectionAt(
            "Can set composing region text",
            ic,
            "farbar",
            3,
        )

        ic.setComposingRegion(1, 4)
        assertTextAndSelectionAt(
            "Can set existing composing region",
            ic,
            "farbar",
            3,
        )

        setComposingText(ic, "rab", 3)
        assertTextAndSelectionAt(
            "Can set new composing region text",
            ic,
            "frabar",
            6, /* checkGecko */
            false,
        )

        finishComposingText(ic)
    }

    // Test getTextBefore/AfterCursor
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_getTextBeforeAfterCursor() {
        setupContent("foobar")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "foobar")

        setSelection(ic, 3, 3)
        assertSelection("Can set selection to range", ic, 3, 3)

        // Test getTextBeforeCursor
        assertThat(
            "Can retrieve text before cursor",
            "foo",
            equalTo(ic.getTextBeforeCursor(3, 0)),
        )

        // Test getTextAfterCursor
        assertThat(
            "Can retrieve text after cursor",
            "bar",
            equalTo(ic.getTextAfterCursor(3, 0)),
        )
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_selectionByArrowKey() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Set initial text", ic, "")

        commitText(ic, "foo", 1) // Selection at end of new text
        assertTextAndSelectionAt("Commit foo text", ic, "foo", 3)

        // backward selection test
        var time = SystemClock.uptimeMillis()
        var shiftKey = KeyEvent(
            time,
            time,
            KeyEvent.ACTION_DOWN,
            KeyEvent.KEYCODE_SHIFT_LEFT,
            0,
        )
        ic.sendKeyEvent(shiftKey)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        processChildEvents()
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        processChildEvents()
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        // No way to get notification for selection on Java side. So sync shadow text
        syncShadowText(ic)
        assertSelection("Set backward select using key event", ic, 3, 0)

        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        // No way to get notification for selection on Java side. So sync shadow text
        syncShadowText(ic)
        assertSelectionAt("Reset selection using key event", ic, 0)

        // forward selection test
        time = SystemClock.uptimeMillis()
        shiftKey = KeyEvent(
            time,
            time,
            KeyEvent.ACTION_DOWN,
            KeyEvent.KEYCODE_SHIFT_LEFT,
            0,
        )
        ic.sendKeyEvent(shiftKey)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_RIGHT)
        processChildEvents()
        pressKey(ic, KeyEvent.KEYCODE_DPAD_RIGHT)
        processChildEvents()
        pressKey(ic, KeyEvent.KEYCODE_DPAD_RIGHT)
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        // No way to get notification for selection on Java side. So sync shadow text
        syncShadowText(ic)
        assertSelection("Set forward select using key event", ic, 0, 3)
    }

    // Test sendKeyEvent
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_sendKeyEvent() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "frabar", 1) // Selection at end of new text
        assertTextAndSelectionAt("Can commit text", ic, "frabar", 6)

        val time = SystemClock.uptimeMillis()
        val shiftKey = KeyEvent(
            time,
            time,
            KeyEvent.ACTION_DOWN,
            KeyEvent.KEYCODE_SHIFT_LEFT,
            0,
        )

        // Wait for selection change
        var promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('selectionchange', r, { once: true }))"
                "#contenteditable" -> "new Promise(r => document.addEventListener('selectionchange', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('selectionchange', r, { once: true }))"
            },
        )

        ic.sendKeyEvent(shiftKey)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        promise.value

        // TODO(m_kato)
        // Since geckoview-junit doesn't attach View, there is no way to wait for correct selection data.
        // So Sync shadow text to avoid failures.
        syncShadowText(ic)
        assertTextAndSelection(
            "Can select using key event",
            ic,
            "frabar",
            6,
            5,
        )

        promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('input', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('input', r, { once: true }))"
            },
        )

        pressKey(ic, KeyEvent.KEYCODE_T)
        promise.value
        assertText("Can type using event", ic, "frabat")
    }

    // Test for Multiple setComposingText with same string length.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_multiple_setComposingText() {
        setupContent("")
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        // Don't wait composition event for this test.
        ic.setComposingText("aaa", 1)
        ic.setComposingText("aaa", 1)
        ic.setComposingText("aab", 1)

        finishComposingText(ic)
        assertTextAndSelectionAt(
            "Multiple setComposingText don't commit composition string",
            ic,
            "aab",
            3,
        )
    }

    // Test for setting large text on text box.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_largeText() {
        val content = (1..102400).map {
            ('a'..'z').random()
        }.joinToString("")
        setupContent(content)
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set large initial text", ic, content, /* checkGecko */ false)
    }

    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.N_MR1)
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_commitContent() {
        if (id == "#input" || id == "#textarea") {
            assertThat(
                "This test is only for contenteditable or designmode",
                true,
                equalTo(true),
            )
            return
        }

        setupContent("")
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Set initial text", ic, "")

        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> """
                        new Promise((resolve, reject) => document.querySelector('$id').contentDocument.addEventListener('input', e => {
                            if (e.inputType == 'insertFromPaste') {
                                resolve();
                            } else {
                                reject();
                            }
                        }, { once: true }))
                """.trimIndent()
                else -> """
                        new Promise((resolve, reject) => document.querySelector('$id').addEventListener('input', e => {
                            if (e.inputType == 'insertFromPaste') {
                                resolve();
                            } else {
                                reject();
                            }
                        }, { once: true }))
                """.trimIndent()
            },
        )

        // InputContentInfo requires content:// uri, so we have to set test data to custom content provider.
        TestContentProvider.setTestData(this.getTestBytes("/assets/www/images/test.gif"), "image/gif")
        val info = InputContentInfo(Uri.parse("content://org.mozilla.geckoview.test.provider/gif"), ClipDescription("test", arrayOf("image/gif")))
        ic.commitContent(info, 0, null)
        promise.value
        assertThat("Input event is fired by inserting image", true, equalTo(true))
    }

    // Bug 1133802, duplication when setting the same composing text more than once.
    @Ignore
    // Disable for frequent failures.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1133802() {
        // TODO:
        // Disable this test for frequent failures. We consider another way to
        // wait/ignore event handling.
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        setComposingText(ic, "foo", 1)
        assertTextAndSelectionAt("Can set the composing text", ic, "foo", 3)
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("foo", 1)
        assertTextAndSelectionAt(
            "Can set the same composing text",
            ic,
            "foo",
            3,
        )
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt(
            "Can set different composing text",
            ic,
            "bar",
            3,
        )
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("bar", 1)
        assertTextAndSelectionAt(
            "Can set the same composing text",
            ic,
            "bar",
            3,
        )
        // Setting same text doesn't fire compositionupdate
        ic.setComposingText("bar", 1)
        assertTextAndSelectionAt(
            "Can set the same composing text again",
            ic,
            "bar",
            3,
        )
        finishComposingText(ic)
        assertTextAndSelectionAt("Can finish composing text", ic, "bar", 3)
    }

    // Bug 1209465, cannot enter ideographic space character by itself (U+3000).
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1209465() {
        // The ideographic space char may trigger font fallback; we don't want that to be async,
        // as the resulting deferred reflow may confuse a following test.
        sessionRule.setPrefsUntilTestEnd(mapOf("gfx.font_rendering.fallback.async" to false))

        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "\u3000", 1)
        assertTextAndSelectionAt(
            "Can commit ideographic space",
            ic,
            "\u3000",
            1,
        )
    }

    // Bug 1275371 - shift+backspace should not forward delete on Android.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1275371() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        ic.beginBatchEdit()
        commitText(ic, "foo", 1)
        setSelection(ic, 1, 1)
        ic.endBatchEdit()
        assertTextAndSelectionAt("Can commit text", ic, "foo", 1)

        val time = SystemClock.uptimeMillis()
        val shiftKey = KeyEvent(
            time,
            time,
            KeyEvent.ACTION_DOWN,
            KeyEvent.KEYCODE_SHIFT_LEFT,
            0,
        )
        ic.sendKeyEvent(shiftKey)

        // Wait for input change
        val promise = mainSession.evaluatePromiseJS(
            when (id) {
                "#designmode" -> "new Promise(r => document.querySelector('$id').contentDocument.addEventListener('input', r, { once: true }))"
                else -> "new Promise(r => document.querySelector('$id').addEventListener('input', r, { once: true }))"
            },
        )

        pressKey(ic, KeyEvent.KEYCODE_DEL)
        promise.value
        assertText("Can backspace with shift+backspace", ic, "oo")

        pressKey(ic, KeyEvent.KEYCODE_DEL)
        ic.sendKeyEvent(KeyEvent.changeAction(shiftKey, KeyEvent.ACTION_UP))
        assertTextAndSelectionAt(
            "Cannot forward delete with shift+backspace",
            ic,
            "oo",
            0,
        )
    }

    // Bug 1490391 - Committing then setting composition can result in duplicates.
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1490391() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Can set initial text", ic, "")

        commitText(ic, "far", 1)
        setComposingText(ic, "bar", 1)
        assertTextAndSelectionAt(
            "Can commit then set composition",
            ic,
            "farbar",
            6,
        )
        setComposingText(ic, "baz", 1)
        assertTextAndSelectionAt(
            "Composition still exists after setting",
            ic,
            "farbaz",
            6,
        )

        finishComposingText(ic)

        // TODO:
        // Call ic.deleteSurroundingText(6, 0) and check result.
        // Actually, no way to wait deleteSurroudingText since this may fire
        // multiple events.
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun sendDummyKeyboardEvent() {
        // unnecessary for designmode
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!
        assertText("Set initial text", ic, "")

        commitText(ic, "foo", 1)
        assertTextAndSelectionAt("commit text and selection", ic, "foo", 3)

        // Dispatching keydown, input and keyup
        val promise =
            mainSession.evaluatePromiseJS(
                """
                new Promise(r => window.addEventListener('keydown', () => {
                                     window.addEventListener('input',() => {
                                         window.addEventListener('keyup', r, { once: true }) },
                                         { once: true }) },
                                     { once: true}))""",
            )
        ic.beginBatchEdit()
        ic.setSelection(0, 3)
        ic.setComposingText("", 1)
        ic.endBatchEdit()
        promise.value
        assertText("empty text", ic, "")
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun editorInfo_default() {
        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val editorInfo = EditorInfo()
        mainSession.textInput.onCreateInputConnection(editorInfo)
        assertThat(
            "Default EditorInfo.inputType",
            editorInfo.inputType,
            equalTo(
                when (id) {
                    "#input" ->
                        InputType.TYPE_CLASS_TEXT or
                            InputType.TYPE_TEXT_FLAG_AUTO_CORRECT or
                            InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE
                    else ->
                        InputType.TYPE_CLASS_TEXT or
                            InputType.TYPE_TEXT_FLAG_CAP_SENTENCES or
                            InputType.TYPE_TEXT_FLAG_AUTO_CORRECT or
                            InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE
                },
            ),
        )
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun editorInfo_defaultByInputType() {
        assumeThat("type attribute is input element only", id, equalTo("#input"))
        // Disable this with WebRender due to unexpected abort by mozilla::gl::GLContext::fTexSubImage2D
        // (Bug 1706688, Bug 1710060 and etc)
        assumeThat(sessionRule.env.isWebrender and sessionRule.env.isDebugBuild, equalTo(false))

        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)
        mainSession.loadTestPath(FORMS5_HTML_PATH)
        mainSession.waitForPageStop()

        for (inputType in listOf("#email1", "#pass1", "#search1", "#tel1", "#url1")) {
            mainSession.evaluateJS("document.querySelector('$inputType').focus()")
            mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

            // IC will be updated asynchronously, so spin event loop
            processChildEvents()
            processParentEvents()

            val editorInfo = EditorInfo()
            val ic = mainSession.textInput.onCreateInputConnection(editorInfo)!!
            assertThat("InputConnection is created correctly", ic, notNullValue())

            // Even if we get IC, new EditorInfo isn't updated yet.
            // We post and wait for empty job to IC thread to flush all IC's job.
            val result = object : GeckoResult<Boolean>() {
                init {
                    val icHandler = mainSession.textInput.getHandler(Handler(Looper.getMainLooper()))
                    icHandler.post({
                        complete(true)
                    })
                }
            }
            sessionRule.waitForResult(result)
            mainSession.textInput.onCreateInputConnection(editorInfo)

            assertThat(
                "EditorInfo.inputType of $inputType",
                editorInfo.inputType,
                equalTo(
                    when (inputType) {
                        "#email1" ->
                            InputType.TYPE_CLASS_TEXT or
                                InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
                        "#pass1" ->
                            InputType.TYPE_CLASS_TEXT or
                                InputType.TYPE_TEXT_VARIATION_PASSWORD
                        "#search1" ->
                            InputType.TYPE_CLASS_TEXT or
                                InputType.TYPE_TEXT_FLAG_AUTO_CORRECT or
                                InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE or
                                InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                        "#tel1" -> InputType.TYPE_CLASS_PHONE
                        "#url1" ->
                            InputType.TYPE_CLASS_TEXT or
                                InputType.TYPE_TEXT_VARIATION_URI
                        else -> 0
                    },
                ),
            )
        }
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun editorInfo_enterKeyHint() {
        // no way to set enterkeyhint on designmode.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))
        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        val values = listOf("enter", "done", "go", "previous", "next", "search", "send")
        for (enterkeyhint in values) {
            mainSession.evaluateJS(
                """
                document.querySelector('$id').enterKeyHint = '$enterkeyhint';
                document.querySelector('$id').focus()""",
            )
            mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

            val editorInfo = EditorInfo()
            mainSession.textInput.onCreateInputConnection(editorInfo)
            assertThat(
                "EditorInfo.imeOptions by $enterkeyhint",
                editorInfo.imeOptions and EditorInfo.IME_MASK_ACTION,
                equalTo(
                    when (enterkeyhint) {
                        "done" -> EditorInfo.IME_ACTION_DONE
                        "go" -> EditorInfo.IME_ACTION_GO
                        "next" -> EditorInfo.IME_ACTION_NEXT
                        "previous" -> EditorInfo.IME_ACTION_PREVIOUS
                        "search" -> EditorInfo.IME_ACTION_SEARCH
                        "send" -> EditorInfo.IME_ACTION_SEND
                        else -> EditorInfo.IME_ACTION_NONE
                    },
                ),
            )

            mainSession.evaluateJS("document.querySelector('$id').blur()")
            mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")
        }
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun editorInfo_autocapitalize() {
        // no way to set autocapitalize on designmode.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        val values = listOf("characters", "none", "sentences", "words", "off", "on")
        for (autocapitalize in values) {
            mainSession.evaluateJS(
                """
                document.querySelector('$id').autocapitalize = '$autocapitalize';
                document.querySelector('$id').focus()""",
            )
            mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

            val editorInfo = EditorInfo()
            mainSession.textInput.onCreateInputConnection(editorInfo)
            assertThat(
                "EditorInfo.inputType by $autocapitalize",
                editorInfo.inputType and 0x00007000,
                equalTo(
                    when (autocapitalize) {
                        "characters" -> InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
                        "on" -> InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                        "sentences" -> InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                        "words" -> InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        else -> 0
                    },
                ),
            )

            mainSession.evaluateJS("document.querySelector('$id').blur()")
            mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")
        }
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun bug1613804_finishComposingText() {
        mainSession.textInput.view = View(InstrumentationRegistry.getInstrumentation().targetContext)

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        textContent = ""
        mainSession.evaluateJS("document.querySelector('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class, "restartInput")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        ic.beginBatchEdit()
        ic.setComposingText("abc", 1)
        ic.endBatchEdit()

        // finishComposingText has to dispatch compositionend event.
        finishComposingText(ic)

        assertText("commit abc", ic, "abc")
    }

    // Bug 1593683 - Cursor is jumping when using the arrow keys in input field on GBoard
    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1593683() {
        setupContent("")

        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        setComposingText(ic, "foo", 1)
        assertTextAndSelectionAt("Can set the composing text", ic, "foo", 3)
        // Arrow key should keep composition then move caret
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        pressKey(ic, KeyEvent.KEYCODE_DPAD_LEFT)
        assertSelection("IME caret is moved to top", ic, 0, 0, /* checkGecko */ false)

        setComposingText(ic, "bar", 1)
        finishComposingText(ic)
        assertText("commit abc", ic, "bar")
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1633621() {
        // no way on designmode.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        setupContent("")
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        mainSession.evaluateJS(
            """
            document.querySelector('$id').addEventListener('input', () => {
                document.querySelector('$id').blur();
                document.querySelector('$id').focus();
            })
         """,
        )

        setComposingText(ic, "b", 1)
        assertTextAndSelectionAt(
            "Don't change caret position after calling blur and focus",
            ic,
            "b",
            1,
        )

        setComposingText(ic, "a", 1)
        assertTextAndSelectionAt(
            "Can set composition string after calling blur and focus",
            ic,
            "ba",
            2,
        )

        pressKey(ic, KeyEvent.KEYCODE_R)
        assertText(
            "Can set input string by keypress after calling blur and focus",
            ic,
            "bar",
        )
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1650705() {
        // no way on designmode.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        setupContent("")
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        commitText(ic, "foo", 1)
        ic.setSelection(0, 3)

        mainSession.evaluateJS(
            """
            input_event_count = 0;
            document.querySelector('$id').addEventListener('input', () => {
                input_event_count++;
            })
        """,
        )

        setComposingText(ic, "barbaz", 1)

        val count = mainSession.evaluateJS("input_event_count") as Double
        assertThat("input event is once", count, equalTo(1.0))

        finishComposingText(ic)
    }

    @WithDisplay(width = 512, height = 512)
    // Child process updates require having a display.
    @Test
    fun inputConnection_bug1767556() {
        setupContent("")
        val ic = mainSession.textInput.onCreateInputConnection(EditorInfo())!!

        // Emulate GBoard's InputConnection API calls
        ic.beginBatchEdit()
        ic.setComposingText("fooba", 1)
        ic.endBatchEdit()
        ic.setComposingText("fooba", 1)
        processChildEvents()

        ic.beginBatchEdit()
        ic.setComposingText("foobaz", 1)
        ic.endBatchEdit()
        ic.setComposingText("foobaz", 1)
        processChildEvents()

        ic.beginBatchEdit()
        ic.setComposingText("foobaz1", 1)
        ic.endBatchEdit()
        ic.setComposingText("foobaz1", 1)
        processChildEvents()

        finishComposingText(ic)
        assertText("commit foobaz1", ic, "foobaz1")
    }
}
