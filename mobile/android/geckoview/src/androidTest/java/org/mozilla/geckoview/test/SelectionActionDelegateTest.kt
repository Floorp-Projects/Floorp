/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.SelectionActionDelegate
import org.mozilla.geckoview.GeckoSession.SelectionActionDelegate.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Point;
import android.graphics.RectF;
import android.os.Build
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.filters.SdkSuppress

import org.hamcrest.Matcher
import org.hamcrest.Matchers.*
import org.json.JSONArray
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.junit.runners.Parameterized.Parameter
import org.junit.runners.Parameterized.Parameters
import org.mozilla.geckoview.GeckoSession

@MediumTest
@RunWith(Parameterized::class)
@WithDisplay(width = 400, height = 400)
class SelectionActionDelegateTest : BaseSessionTest() {
    val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

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
                arrayOf("#designmode", ContentType.IFRAME, "consectetur", true),
                arrayOf("#x-input", ContentType.EDITABLE_ELEMENT, "adipisci", true))
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

    @Before
    fun setup() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Writing clipboard requires foreground on Android 10.
            activityRule.scenario.onActivity { activity ->
                activity.onWindowFocusChanged(true)
            }
        }
    }

    /** Generic tests for each content type. */

    @Test fun request() {
        if (editable) {
            withClipboard ("text") {
                testThat(selectedContent, {}, hasShowActionRequest(
                        FLAG_IS_EDITABLE, arrayOf(ACTION_COLLAPSE_TO_START, ACTION_COLLAPSE_TO_END,
                                                  ACTION_COPY, ACTION_CUT, ACTION_DELETE,
                                                  ACTION_HIDE, ACTION_PASTE)))
            }
        } else {
            testThat(selectedContent, {}, hasShowActionRequest(
                    0, arrayOf(ACTION_COPY, ACTION_HIDE, ACTION_SELECT_ALL,
                                           ACTION_UNSELECT)))
        }
    }

    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.O)
    @Test fun request_html() {
        if (editable) {
            withHtmlClipboard ("text", "<bold>text</bold>") {
                if (type != ContentType.EDITABLE_ELEMENT) {
                    testThat(selectedContent, {}, hasShowActionRequest(
                            FLAG_IS_EDITABLE, arrayOf(ACTION_COLLAPSE_TO_START, ACTION_COLLAPSE_TO_END,
                                                      ACTION_COPY, ACTION_CUT, ACTION_DELETE,
                                                      ACTION_HIDE, ACTION_PASTE,
                                                      ACTION_PASTE_AS_PLAIN_TEXT)))
                } else {
                    testThat(selectedContent, {}, hasShowActionRequest(
                            FLAG_IS_EDITABLE, arrayOf(ACTION_COLLAPSE_TO_START, ACTION_COLLAPSE_TO_END,
                                                      ACTION_COPY, ACTION_CUT, ACTION_DELETE,
                                                      ACTION_HIDE, ACTION_PASTE)))
                }
            }
        } else {
            testThat(selectedContent, {}, hasShowActionRequest(
                    0, arrayOf(ACTION_COPY, ACTION_HIDE, ACTION_SELECT_ALL,
                                           ACTION_UNSELECT)))
        }
    }

    @Test fun request_collapsed() = assumingEditable(true) {
        withClipboard ("text") {
            testThat(collapsedContent, {}, hasShowActionRequest(
                    FLAG_IS_EDITABLE or FLAG_IS_COLLAPSED,
                    arrayOf(ACTION_HIDE, ACTION_PASTE, ACTION_SELECT_ALL)))
        }
    }

    @Test fun request_noClipboard() = assumingEditable(true) {
        withClipboard("") {
            testThat(collapsedContent, {}, hasShowActionRequest(
                    FLAG_IS_EDITABLE or FLAG_IS_COLLAPSED,
                    arrayOf(ACTION_HIDE, ACTION_SELECT_ALL)))
        }
    }

    @Test fun hide() = testThat(selectedContent, withResponse(ACTION_HIDE), clearsSelection())

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

    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.O)
    @Test fun pasteAsPlainText() = assumingEditable(true) {
        assumeThat("Paste as plain text works on content editable", type, not(equalTo(ContentType.EDITABLE_ELEMENT)));

        withHtmlClipboard("pasted", "<bold>pasted</bold>") {
            testThat(selectedContent, withResponse(ACTION_PASTE_AS_PLAIN_TEXT), changesContentTo("pasted"))
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

    @Test fun pagehide() {
        // Navigating to another page should hide selection action.
        testThat(selectedContent, { mainSession.loadTestPath(HELLO_HTML_PATH) }, clearsSelection())
    }

    @Test fun deactivate() {
        // Blurring the window should hide selection action.
        testThat(selectedContent, { mainSession.setFocused(false) }, clearsSelection())
        mainSession.setFocused(true)
    }

    @NullDelegate(GeckoSession.SelectionActionDelegate::class)
    @Test fun clearDelegate() {
        var counter = 0
        mainSession.selectionActionDelegate = object : SelectionActionDelegate {
            override fun onHideAction(session: GeckoSession, reason: Int) {
                counter++
            }
        }

        mainSession.selectionActionDelegate = null
        assertThat("Hide action should be called when clearing delegate",
                   counter, equalTo(1))
    }

    @Test fun compareClientRect() {
        val jsCssReset = """(function() {
            document.querySelector('${id}').style.display = "block";
            document.querySelector('${id}').style.border = "0";
            document.querySelector('${id}').style.padding = "0";
            document.querySelector('${id}').offsetHeight; // flush layout
        })()"""
        val jsBorder10pxPadding10px = """(function() {
            document.querySelector('${id}').style.display = "block";
            document.querySelector('${id}').style.border = "10px solid";
            document.querySelector('${id}').style.padding = "10px";
            document.querySelector('${id}').offsetHeight; // flush layout
        })()"""
        val expectedDiff = RectF(20f, 20f, 20f, 20f) // left, top, right, bottom
        testClientRect(selectedContent, jsCssReset, jsBorder10pxPadding10px, expectedDiff)
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun clipboardReadAllow() {
        assumeThat("Unnecessary to run multiple times", id, equalTo("#text"));

        sessionRule.setPrefsUntilTestEnd(mapOf("dom.events.asyncClipboard.readText" to true))

        val url = createTestUrl(CLIPBOARD_READ_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        // Select allow
        val result = GeckoResult<Void>()
        mainSession.delegateDuringNextWait(object : SelectionActionDelegate, PromptDelegate {
            @AssertCalled(count = 1)
            override fun onShowClipboardPermissionRequest(
                    session: GeckoSession, perm: ClipboardPermission):
                    GeckoResult<AllowOrDeny> {
                assertThat("URI should match", perm.uri, startsWith(url))
                assertThat("Type should match", perm.type,
                        equalTo(SelectionActionDelegate.PERMISSION_CLIPBOARD_READ))
                assertThat("screenPoint should match", perm.screenPoint, equalTo(Point(50, 50)))
                return GeckoResult.allow()
            }

            @AssertCalled(count = 1, order = [2])
            override fun onAlertPrompt(
                    session: GeckoSession, prompt: PromptDelegate.AlertPrompt):
                    GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "allow", equalTo(prompt.message))
                result.complete(null)
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        mainSession.synthesizeTap(50, 50) // Provides user activation.
        sessionRule.waitForResult(result)
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun clipboardReadDeny() {
        assumeThat("Unnecessary to run multiple times", id, equalTo("#text"));

        sessionRule.setPrefsUntilTestEnd(mapOf("dom.events.asyncClipboard.readText" to true))

        val url = createTestUrl(CLIPBOARD_READ_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        // Select deny
        val result = GeckoResult<Void>()
        mainSession.delegateDuringNextWait(object : SelectionActionDelegate, PromptDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onShowClipboardPermissionRequest(
                    session: GeckoSession, perm: ClipboardPermission):
                    GeckoResult<AllowOrDeny>? {
                assertThat("URI should match", perm.uri, startsWith(url))
                assertThat("Type should match", perm.type,
                        equalTo(SelectionActionDelegate.PERMISSION_CLIPBOARD_READ))
                return GeckoResult.deny()
            }

            @AssertCalled(count = 1, order = [2])
            override fun onAlertPrompt(
                    session: GeckoSession, prompt: PromptDelegate.AlertPrompt):
                    GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "deny", equalTo(prompt.message))
                result.complete(null)
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        mainSession.synthesizeTap(50, 50) // Provides user activation.
        sessionRule.waitForResult(result)
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun clipboardReadDeactivate() {
        assumeThat("Unnecessary to run multiple times", id, equalTo("#text"));

        sessionRule.setPrefsUntilTestEnd(mapOf("dom.events.asyncClipboard.readText" to true))

        val url = createTestUrl(CLIPBOARD_READ_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        val result = GeckoResult<Void>()
        mainSession.delegateDuringNextWait(object : SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowClipboardPermissionRequest(
                    session: GeckoSession, perm: ClipboardPermission):
                    GeckoResult<AllowOrDeny>? {
                assertThat("Type should match", perm.type,
                        equalTo(SelectionActionDelegate.PERMISSION_CLIPBOARD_READ))
                result.complete(null)
                return GeckoResult()
            }
        });

        mainSession.synthesizeTap(50, 50) // Provides user activation.
        sessionRule.waitForResult(result)

        mainSession.delegateDuringNextWait(object : SelectionActionDelegate {
            @AssertCalled
            override fun onDismissClipboardPermissionRequest(session: GeckoSession) {
            }
        });

        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
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
                         respondingWith: (Selection) -> Unit,
                         result: (SelectedContent) -> Unit,
                         vararg sideEffects: (SelectedContent) -> Unit) {

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        content.focus()

        // Show selection actions for collapsed selections, so we can test them.
        // Also, always show accessible carets / selection actions for changes due to JS calls.
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "geckoview.selection_action.show_on_focus" to true,
                "layout.accessiblecaret.script_change_update_mode" to 2))

        mainSession.delegateDuringNextWait(object : SelectionActionDelegate {
            override fun onShowActionRequest(session: GeckoSession, selection: GeckoSession.SelectionActionDelegate.Selection) {
                respondingWith(selection)
            }
        })

        content.select()
        mainSession.waitUntilCalled(object : SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: Selection) {
                assertThat("Initial content should match",
                           selection.text, equalTo(content.initialContent))
            }
        })

        result(content)
        sideEffects.forEach { it(content) }
    }

    private fun testClientRect(content: SelectedContent,
                               initialJsA: String,
                               initialJsB: String,
                               expectedDiff: RectF) {

        // Show selection actions for collapsed selections, so we can test them.
        // Also, always show accessible carets / selection actions for changes due to JS calls.
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "geckoview.selection_action.show_on_focus" to true,
                "layout.accessiblecaret.script_change_update_mode" to 2))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        val requestClientRect: (String) -> RectF = {
            mainSession.reload()
            mainSession.waitForPageStop()

            mainSession.evaluateJS(it)
            content.focus()

            var clientRect = RectF()
            content.select()
            mainSession.waitUntilCalled(object : SelectionActionDelegate {
                @AssertCalled(count = 1)
                override fun onShowActionRequest(session: GeckoSession, selection: Selection) {
                    clientRect = selection.clientRect!!
                }
            })

            clientRect
        }

        val clientRectA = requestClientRect(initialJsA)
        val clientRectB = requestClientRect(initialJsB)

        val fuzzyEqual = { a: Float, b: Float, e: Float -> Math.abs(a + e - b) <= 1 }
        val result = fuzzyEqual(clientRectA.top, clientRectB.top, expectedDiff.top)
                  && fuzzyEqual(clientRectA.left, clientRectB.left, expectedDiff.left)
                  && fuzzyEqual(clientRectA.width(), clientRectB.width(), expectedDiff.width())
                  && fuzzyEqual(clientRectA.height(), clientRectB.height(), expectedDiff.height())

        assertThat("Selection rect is not at expected location. a$clientRectA b$clientRectB",
                   result, equalTo(true))
    }


    /** Helpers. */

    private val clipboard by lazy {
        InstrumentationRegistry.getInstrumentation().targetContext.getSystemService(Context.CLIPBOARD_SERVICE)
                as ClipboardManager
    }

    private fun withClipboard(content: String = "", lambda: () -> Unit) {
        val oldClip = clipboard.primaryClip
        try {
            if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P && content.isEmpty()) {
                clipboard.clearPrimaryClip()
            } else {
                clipboard.setPrimaryClip(ClipData.newPlainText("", content))
            }

            sessionRule.addExternalDelegateUntilTestEnd(
                    ClipboardManager.OnPrimaryClipChangedListener::class,
                    clipboard::addPrimaryClipChangedListener,
                    clipboard::removePrimaryClipChangedListener,
                    ClipboardManager.OnPrimaryClipChangedListener {})
            lambda()
        } finally {
            clipboard.setPrimaryClip(oldClip ?: ClipData.newPlainText("", ""))
        }
    }

    private fun withHtmlClipboard(plainText: String = "", html: String = "", lambda: () -> Unit) {
        val oldClip = clipboard.primaryClip
        try {
            clipboard.setPrimaryClip(ClipData.newHtmlText("", plainText, html))

            sessionRule.addExternalDelegateUntilTestEnd(
                    ClipboardManager.OnPrimaryClipChangedListener::class,
                    clipboard::addPrimaryClipChangedListener,
                    clipboard::removePrimaryClipChangedListener,
                    ClipboardManager.OnPrimaryClipChangedListener {})
            lambda()
        } finally {
            clipboard.setPrimaryClip(oldClip ?: ClipData.newPlainText("", ""))
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
                document.querySelector('$id').firstChild, 0,
                document.querySelector('$id').firstChild, $to)""")
        }

        override fun select() = selectTo(initialContent.length)

        override val content: String get() {
            return mainSession.evaluateJS("document.querySelector('$id').textContent") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            if (mainSession.evaluateJS("""
                document.getSelection().anchorNode !== document.querySelector('$id').firstChild ||
                document.getSelection().focusNode !== document.querySelector('$id').firstChild""") as Boolean) {
                return Pair(-1, -1)
            }
            val offsets = mainSession.evaluateJS("""[
                document.getSelection().anchorOffset,
                document.getSelection().focusOffset]""") as JSONArray
            return Pair(offsets[0] as Int, offsets[1] as Int)
        }
    }

    inner class CollapsedDiv(id: String) : SelectedDiv(id, "") {
        override fun select() = selectTo(0)
    }

    open inner class SelectedEditableElement(
            val id: String, override val initialContent: String) : SelectedContent {
        override fun focus() {
            mainSession.waitForJS("document.querySelector('$id').focus()")
        }

        override fun select() {
            mainSession.evaluateJS("document.querySelector('$id').select()")
        }

        override val content: String get() {
            return mainSession.evaluateJS("document.querySelector('$id').value") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            val offsets = mainSession.evaluateJS(
                    """[ document.querySelector('$id').selectionStart,
                        |document.querySelector('$id').selectionEnd ]""".trimMargin()) as JSONArray
            return Pair(offsets[0] as Int, offsets[1] as Int)
        }
    }

    inner class CollapsedEditableElement(id: String) : SelectedEditableElement(id, "") {
        override fun select() {
            mainSession.evaluateJS("document.querySelector('$id').setSelectionRange(0, 0)")
        }
    }

    open inner class SelectedFrame(val id: String,
                                   override val initialContent: String) : SelectedContent {
        override fun focus() {
            mainSession.evaluateJS("document.querySelector('$id').contentWindow.focus()")
        }

        protected fun selectTo(to: Int) {
            mainSession.evaluateJS("""(function() {
                    var doc = document.querySelector('$id').contentDocument;
                    var text = doc.body.firstChild;
                    doc.getSelection().setBaseAndExtent(text, 0, text, $to);
                })()""")
        }

        override fun select() = selectTo(initialContent.length)

        override val content: String get() {
            return mainSession.evaluateJS("document.querySelector('$id').contentDocument.body.textContent") as String
        }

        override val selectionOffsets: Pair<Int, Int> get() {
            val offsets = mainSession.evaluateJS("""(function() {
                    var sel = document.querySelector('$id').contentDocument.getSelection();
                    var text = document.querySelector('$id').contentDocument.body.firstChild;
                    if (sel.anchorNode !== text || sel.focusNode !== text) {
                        return [-1, -1];
                    }
                    return [sel.anchorOffset, sel.focusOffset];
                })()""") as JSONArray
            return Pair(offsets[0] as Int, offsets[1] as Int)
        }
    }

    inner class CollapsedFrame(id: String) : SelectedFrame(id, "") {
        override fun select() = selectTo(0)
    }


    /** Lambda for responding with certain actions. */

    private fun withResponse(vararg actions: String): (Selection) -> Unit {
        var responded = false
        return { response ->
            if (!responded) {
                responded = true
                actions.forEach { response.execute(it) }
            }
        }
    }


    /** Lambdas for asserting the results of actions. */

    private fun hasShowActionRequest(expectedFlags: Int,
                                     expectedActions: Array<out String>) = { it: SelectedContent ->
        mainSession.forCallbacksDuringWait(object : SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: GeckoSession.SelectionActionDelegate.Selection) {
                assertThat("Selection text should be valid",
                           selection.text, equalTo(it.initialContent))
                assertThat("Selection flags should be valid",
                           selection.flags, equalTo(expectedFlags))
                assertThat("Selection rect should be valid",
                           selection.clientRect!!.isEmpty, equalTo(false))
                assertThat("Actions must be valid", selection.availableActions.toTypedArray(),
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
        sessionRule.waitUntilCalled(object : SelectionActionDelegate {
            @AssertCalled(count = 1)
            override fun onShowActionRequest(session: GeckoSession, selection: Selection) {
                assertThat("New selection text should match", selection.text, matcher)
            }
        })
    }

    private fun clearsSelection() = { _: SelectedContent ->
        sessionRule.waitUntilCalled(object : SelectionActionDelegate {
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
