/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.SelectionActionDelegate.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest

import org.hamcrest.Matcher
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.junit.runners.Parameterized.Parameter
import org.junit.runners.Parameterized.Parameters

@MediumTest
@RunWith(Parameterized::class)
@WithDevToolsAPI
@WithDisplay(width = 100, height = 100)
class SelectionActionDelegateTest : BaseSessionTest() {
    enum class ContentType {
        DIV, EDITABLE_ELEMENT, IFRAME
    }

    companion object {
        @get:Parameters(name = "{0}")
        @JvmStatic
        val parameters: List<Array<out Any>> = listOf(
                arrayOf("#text", ContentType.DIV, "lorem", false),
                arrayOf("#input", ContentType.EDITABLE_ELEMENT, "ipsum", true),
                arrayOf("#textarea", ContentType.EDITABLE_ELEMENT, "dolor", true),
                arrayOf("#contenteditable", ContentType.DIV, "sit", true),
                arrayOf("#iframe", ContentType.IFRAME, "amet", false),
                arrayOf("#designmode", ContentType.IFRAME, "consectetur", true))
    }

    @field:Parameter(0) @JvmField var id: String = ""
    @field:Parameter(1) @JvmField var type: ContentType = ContentType.DIV
    @field:Parameter(2) @JvmField var initialContent: String = ""
    @field:Parameter(3) @JvmField var editable: Boolean = false

    private val selectedContent by lazy {
        when (type) {
            ContentType.DIV -> SelectedDiv(id, initialContent)
            ContentType.EDITABLE_ELEMENT -> SelectedEditableElement(id, initialContent)
            ContentType.IFRAME -> SelectedFrame(id, initialContent)
        }
    }

    private val collapsedContent by lazy {
        when (type) {
            ContentType.DIV -> CollapsedDiv(id)
            ContentType.EDITABLE_ELEMENT -> CollapsedEditableElement(id)
            ContentType.IFRAME -> CollapsedFrame(id)
        }
    }


    /** Generic tests for each content type. */

    @Test fun request() {
        if (editable) {
            withClipboard ("text") {
                testThat(selectedContent, {}, hasShowActionRequest(
                        FLAG_IS_EDITABLE, arrayOf(ACTION_COLLAPSE_TO_START, ACTION_COLLAPSE_TO_END,
                                                  ACTION_COPY, ACTION_CUT, ACTION_DELETE,
                                                  ACTION_PASTE, ACTION_SELECT_ALL)))
            }
        } else {
            testThat(selectedContent, {}, hasShowActionRequest(
                    0, arrayOf(ACTION_COPY, ACTION_SELECT_ALL, ACTION_UNSELECT)))
        }
    }

    @Test fun request_collapsed() = assumingEditable(true) {
        withClipboard ("text") {
            testThat(collapsedContent, {}, hasShowActionRequest(
                    FLAG_IS_EDITABLE or FLAG_IS_COLLAPSED,
                    arrayOf(ACTION_PASTE, ACTION_SELECT_ALL)))
        }
    }

    @Test fun request_noClipboard() = assumingEditable(true) {
        withClipboard("") {
            testThat(collapsedContent, {}, hasShowActionRequest(
                    FLAG_IS_EDITABLE or FLAG_IS_COLLAPSED, arrayOf(ACTION_SELECT_ALL)))
        }
    }

    @Test fun cut() = assumingEditable(true) {
        withClipboard("") {
            testThat(selectedContent, withResponse(ACTION_CUT), copiesText(), deletesContent())
        }
    }

    @Test fun copy() = withClipboard("") {
        testThat(selectedContent, withResponse(ACTION_COPY), copiesText())
    }

    @Test fun paste() = assumingEditable(true) {
        withClipboard("pasted") {
            testThat(selectedContent, withResponse(ACTION_PASTE), changesContentTo("pasted"))
        }
    }

    @Test fun delete() = assumingEditable(true) {
        testThat(selectedContent, withResponse(ACTION_DELETE), deletesContent())
    }

    @Test fun selectAll() {
        if (type == ContentType.DIV && !editable) {
            // "Select all" for non-editable div means selecting the whole document.
            testThat(selectedContent, withResponse(ACTION_SELECT_ALL), changesSelectionTo(
                    both(containsString(selectedContent.initialContent))
                            .and(not(equalTo(selectedContent.initialContent)))))
        } else {
            testThat(if (editable) collapsedContent else selectedContent,
                     withResponse(ACTION_SELECT_ALL),
                     changesSelectionTo(selectedContent.initialContent))
        }
    }

    @Test fun unselect() = assumingEditable(false) {
        testThat(selectedContent, withResponse(ACTION_UNSELECT), clearsSelection())
    }

    @Test fun multipleActions() = assumingEditable(false) {
        withClipboard("") {
            testThat(selectedContent, withResponse(ACTION_COPY, ACTION_UNSELECT),
                     copiesText(), clearsSelection())
        }
    }

    @Test fun collapseToStart() = assumingEditable(true) {
        testThat(selectedContent, withResponse(ACTION_COLLAPSE_TO_START), hasSelectionAt(0))
    }

    @Test fun collapseToEnd() = assumingEditable(true) {
        testThat(selectedContent, withResponse(ACTION_COLLAPSE_TO_END),
                 hasSelectionAt(selectedContent.initialContent.length))
    }


    /** Interface that defines behavior for a particular type of content */
    private interface SelectedContent {
        fun focus() {}
        fun select() {}
        val initialContent: String
        val content: String
        val selectionOffsets: Pair<Int, Int>
    }

    /** Main method that performs test logic. */
    private fun testThat(content: SelectedContent,
                         respondingWith: (GeckoResponse<String>) -> Unit,
                         result: (SelectedContent) -> Unit,
                         vararg sideEffects: (SelectedContent) -> Unit) {

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "layout.accessiblecaret.enabled_on_touch" to true))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        content.focus()

        // Show selection actions for collapsed selections, so we can test them.
        // Also, always show accessible carets / selection actions for changes due to JS calls.
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "geckoview.selection_action.show_on_focus" to true,
                "layout.accessiblecaret.script_change_update_mode" to 2))

        mainSession.delegateDuringNextWait(object : Callbacks.SelectionActionDelegate {
            override fun onShowActionRequest(session: GeckoSession, selection: GeckoSession.SelectionActionDelegate.Selection, actions: Array<out String>, response: GeckoResponse<String>) {
                respondingWith(response)
            }
        })

        content.select()
        mainSession.waitUntilCalled(object : Callbacks.SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: Selection, actions: Array<out String>, response: GeckoResponse<String>) {
                assertThat("Initial content should match",
                           selection.text, equalTo(content.initialContent))
            }
        })

        result(content)
        sideEffects.forEach { it(content) }
    }


    /** Helpers. */

    private val clipboard by lazy {
        InstrumentationRegistry.getTargetContext().getSystemService(Context.CLIPBOARD_SERVICE)
                as ClipboardManager
    }

    private fun withClipboard(content: String = "", lambda: () -> Unit) {
        val oldClip = clipboard.primaryClip
        try {
            clipboard.primaryClip = ClipData.newPlainText("", content)

            sessionRule.addExternalDelegateUntilTestEnd(
                    ClipboardManager.OnPrimaryClipChangedListener::class,
                    clipboard::addPrimaryClipChangedListener,
                    clipboard::removePrimaryClipChangedListener,
                    ClipboardManager.OnPrimaryClipChangedListener {})
            lambda()
        } finally {
            clipboard.primaryClip = oldClip ?: ClipData.newPlainText("", "")
        }
    }

    private fun assumingEditable(editable: Boolean, lambda: (() -> Unit)? = null) {
        assumeThat("Assuming is ${if (editable) "" else "not "}editable",
                   this.editable, equalTo(editable))
        lambda?.invoke()
    }


    /** Behavior objects for different content types */

    open inner class SelectedDiv(val id: String,
                                 override val initialContent: String) : SelectedContent {
        protected fun selectTo(to: Int) {
            mainSession.evaluateJS("""document.getSelection().setBaseAndExtent(
                $('$id').firstChild, 0, $('$id').firstChild, $to)""")
        }

        override fun select() = selectTo(initialContent.length)

        override val content: String get() {
            return mainSession.evaluateJS("$('$id').textContent") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            if (mainSession.evaluateJS("""
                document.getSelection().anchorNode !== $('$id').firstChild ||
                document.getSelection().focusNode !== $('$id').firstChild""") as Boolean) {
                return Pair(-1, -1)
            }
            val offsets = mainSession.evaluateJS("""[
                document.getSelection().anchorOffset,
                document.getSelection().focusOffset]""").asJSList<Double>()
            return Pair(offsets[0].toInt(), offsets[1].toInt())
        }
    }

    inner class CollapsedDiv(id: String) : SelectedDiv(id, "") {
        override fun select() = selectTo(0)
    }

    open inner class SelectedEditableElement(
            val id: String, override val initialContent: String) : SelectedContent {
        override fun focus() {
            mainSession.waitForJS("$('$id').focus()")
        }

        override fun select() {
            mainSession.evaluateJS("$('$id').select()")
        }

        override val content: String get() {
            return mainSession.evaluateJS("$('$id').value") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            val offsets = mainSession.evaluateJS(
                    "[ $('$id').selectionStart, $('$id').selectionEnd ]").asJSList<Double>()
            return Pair(offsets[0].toInt(), offsets[1].toInt())
        }
    }

    inner class CollapsedEditableElement(id: String) : SelectedEditableElement(id, "") {
        override fun select() {
            mainSession.evaluateJS("$('$id').setSelectionRange(0, 0)")
        }
    }

    open inner class SelectedFrame(val id: String,
                                   override val initialContent: String) : SelectedContent {
        override fun focus() {
            mainSession.evaluateJS("$('$id').contentWindow.focus()")
        }

        protected fun selectTo(to: Int) {
            mainSession.evaluateJS("""(function() {
                    var doc = $('$id').contentDocument;
                    var text = doc.body.firstChild;
                    doc.getSelection().setBaseAndExtent(text, 0, text, $to);
                })()""")
        }

        override fun select() = selectTo(initialContent.length)

        override val content: String get() {
            return mainSession.evaluateJS("$('$id').contentDocument.body.textContent") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            val offsets = mainSession.evaluateJS("""(function() {
                    var sel = $('$id').contentDocument.getSelection();
                    var text = $('$id').contentDocument.body.firstChild;
                    if (sel.anchorNode !== text || sel.focusNode !== text) {
                        return [-1, -1];
                    }
                    return [sel.anchorOffset, sel.focusOffset];
                })()""").asJSList<Double>()
            return Pair(offsets[0].toInt(), offsets[1].toInt())
        }
    }

    inner class CollapsedFrame(id: String) : SelectedFrame(id, "") {
        override fun select() = selectTo(0)
    }


    /** Lambda for responding with certain actions. */

    private fun withResponse(vararg actions: String): (GeckoResponse<String>) -> Unit {
        var responded = false
        return { response ->
            if (!responded) {
                responded = true
                actions.forEach { response.respond(it) }
            }
        }
    }


    /** Lambdas for asserting the results of actions. */

    private fun hasShowActionRequest(expectedFlags: Int,
                                     expectedActions: Array<out String>) = { it: SelectedContent ->
        mainSession.forCallbacksDuringWait(object : Callbacks.SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: GeckoSession.SelectionActionDelegate.Selection, actions: Array<out String>, response: GeckoResponse<String>) {
                assertThat("Selection text should be valid",
                           selection.text, equalTo(it.initialContent))
                assertThat("Selection flags should be valid",
                           selection.flags, equalTo(expectedFlags))
                assertThat("Selection rect should be valid",
                           selection.clientRect.isEmpty, equalTo(false))
                assertThat("Actions must be valid", actions,
                           arrayContainingInAnyOrder(*expectedActions))
            }
        })
    }

    private fun copiesText() = { it: SelectedContent ->
        sessionRule.waitUntilCalled(ClipboardManager.OnPrimaryClipChangedListener {
            assertThat("Clipboard should contain correct text",
                       clipboard.primaryClip?.getItemAt(0)?.text,
                       hasToString(it.initialContent))
        })
    }

    private fun changesSelectionTo(text: String) = changesSelectionTo(equalTo(text))

    private fun changesSelectionTo(matcher: Matcher<String>) = { _: SelectedContent ->
        sessionRule.waitUntilCalled(object : Callbacks.SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: Selection, actions: Array<out String>, response: GeckoResponse<String>) {
                assertThat("New selection text should match", selection.text, matcher)
            }
        })
    }

    private fun clearsSelection() = { _: SelectedContent ->
        sessionRule.waitUntilCalled(object : Callbacks.SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onHideAction(session: GeckoSession, reason: Int) {
                assertThat("Hide reason should be correct",
                           reason, equalTo(HIDE_REASON_NO_SELECTION))
            }
        })
    }

    private fun hasSelectionAt(offset: Int) = hasSelectionAt(offset, offset)

    private fun hasSelectionAt(start: Int, end: Int) = { it: SelectedContent ->
        assertThat("Selection offsets should match",
                   it.selectionOffsets, equalTo(Pair(start, end)))
    }

    private fun deletesContent() = changesContentTo("")

    private fun changesContentTo(content: String) = { it: SelectedContent ->
        assertThat("Changed content should match", it.content, equalTo(content))
    }
}
