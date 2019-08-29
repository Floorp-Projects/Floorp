/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.app.ActivityManager
import android.content.Context
import android.graphics.Matrix
import android.graphics.SurfaceTexture
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.LocaleList
import android.os.Process
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import android.support.annotation.AnyThread
import android.support.test.filters.MediumTest
import android.support.test.filters.SdkSuppress
import android.support.test.runner.AndroidJUnit4
import android.text.InputType
import android.util.Pair
import android.util.SparseArray
import android.view.Surface
import android.view.View
import android.view.ViewStructure
import android.view.autofill.AutofillId
import android.view.autofill.AutofillValue
import android.widget.EditText
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.gecko.GeckoAppShell
import org.mozilla.geckoview.SlowScriptResponse
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate


@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateTest : BaseSessionTest() {
    @Test fun titleChange() {
        sessionRule.session.loadTestPath(TITLE_CHANGE_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 2)
            override fun onTitleChange(session: GeckoSession, title: String?) {
                assertThat("Title should match", title,
                           equalTo(forEachCall("Title1", "Title2")))
            }
        })
    }

    @Test fun download() {
        // disable test on pgo for frequently failing Bug 1543355
        assumeThat(sessionRule.env.isDebugBuild, equalTo(true))
        sessionRule.session.loadTestPath(DOWNLOAD_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate, Callbacks.ContentDelegate {

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest):
                                       GeckoResult<AllowOrDeny>? {
                return null
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onExternalResponse(session: GeckoSession, response: GeckoSession.WebResponseInfo) {
                assertThat("Uri should start with data:", response.uri, startsWith("data:"))
                assertThat("Content type should match", response.contentType, equalTo("text/plain"))
                assertThat("Content length should be non-zero", response.contentLength, greaterThan(0L))
                assertThat("Filename should match", response.filename, equalTo("download.txt"))
            }
        })
    }

    @IgnoreCrash
    @Test fun crashContent() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.isX86,
                   equalTo(false))

        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onCrash(session: GeckoSession) {
                assertThat("Session should be closed after a crash",
                           session.isOpen, equalTo(false))
            }
        })

        // Recover immediately
        mainSession.open()
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitUntilCalled(object: Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @IgnoreCrash
    @WithDisplay(width = 10, height = 10)
    @Test fun crashContent_tapAfterCrash() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.isX86,
                   equalTo(false))

        mainSession.delegateUntilTestEnd(object : Callbacks.ContentDelegate {
            override fun onCrash(session: GeckoSession) {
                mainSession.open()
                mainSession.loadTestPath(HELLO_HTML_PATH)
            }
        })

        mainSession.synthesizeTap(5, 5)
        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitForPageStop()

        mainSession.synthesizeTap(5, 5)
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @IgnoreCrash
    @Test fun crashContentMultipleSessions() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.isX86,
                   equalTo(false))

        // XXX we need to make sure all sessions in a given content process receive onCrash().
        // If we add multiple content processes, this test will need to be fixed to ensure the
        // test sessions go into the same one.
        val newSession = sessionRule.createOpenSession()

        // We can inadvertently catch the `onCrash` call for the cached session if we don't specify
        // individual sessions here. Therefore, assert 'onCrash' is called for the two sessions
        // individually.
        val mainSessionCrash = GeckoResult<Void>()
        val newSessionCrash = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : Callbacks.ContentDelegate {
            fun reportCrash(session: GeckoSession) {
                if (session == mainSession) {
                    mainSessionCrash.complete(null)
                } else if (session == newSession) {
                    newSessionCrash.complete(null)
                }
            }
            // Slower devices may not catch crashes in a timely manner, so we check to see
            // if either `onKill` or `onCrash` is called
            override fun onCrash(session: GeckoSession) {
                reportCrash(session)
            }
            override fun onKill(session: GeckoSession) {
                reportCrash(session)
            }
        })

        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        mainSession.loadUri(CONTENT_CRASH_URL)

        sessionRule.waitForResult(newSessionCrash)
        sessionRule.waitForResult(mainSessionCrash)
    }

    @AnyThread
    fun killContentProcess() {
        val context = GeckoAppShell.getApplicationContext()
        val manager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        for (info in manager.runningAppProcesses) {
            if (info.processName.endsWith(":tab")) {
                Process.killProcess(info.pid)
            }
        }
    }

    @IgnoreCrash
    @Test fun killContent() {
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.isX86,
                equalTo(false))

        killContentProcess()
        mainSession.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onKill(session: GeckoSession) {
                assertThat("Session should be closed after being killed",
                        session.isOpen, equalTo(false))
            }
        })

        mainSession.open()
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitUntilCalled(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully", success, equalTo(true))
            }
        })
    }

    @IgnoreCrash
    @Test fun killContentMultipleSessions() {
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.isX86,
                equalTo(false))

        val newSession = sessionRule.createOpenSession()
        killContentProcess()

        val remainingSessions = mutableListOf(newSession, mainSession)
        while (remainingSessions.isNotEmpty()) {
            sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
                @AssertCalled(count = 1)
                override fun onKill(session: GeckoSession) {
                    remainingSessions.remove(session)
                }
            })
        }
    }

    // TextInputDelegateTest is parameterized, so we put this test under ContentDelegateTest.
    @SdkSuppress(minSdkVersion = 23)
    @Test fun autofill() {
        // Test parts of the Oreo auto-fill API; there is another autofill test in
        // SessionAccessibility for a11y auto-fill support.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            // For the root document and the iframe document, each has a form group and
            // a group for inputs outside of forms, so the total count is 4.
            @AssertCalled(count = 4)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
            }
        })

        val autoFills = mapOf(
                "#user1" to "bar", "#user2" to "bar") +
                if (Build.VERSION.SDK_INT >= 26) mapOf(
                        "#pass1" to "baz", "#pass2" to "baz", "#email1" to "a@b.c",
                        "#number1" to "24", "#tel1" to "42")
                else mapOf(
                        "#pass1" to "bar", "#pass2" to "bar", "#email1" to "bar",
                        "#number1" to "", "#tel1" to "bar")

        // Set up promises to monitor the values changing.
        val promises = autoFills.flatMap { entry ->
            // Repeat each test with both the top document and the iframe document.
            arrayOf("document", "document.querySelector('#iframe').contentDocument").map { doc ->
                mainSession.evaluatePromiseJS("""new Promise(resolve =>
                    $doc.querySelector('${entry.key}').addEventListener(
                      'input', event => {
                        let eventInterface =
                          event instanceof InputEvent ? "InputEvent" :
                          event instanceof UIEvent ? "UIEvent" :
                          event instanceof Event ? "Event" : "Unknown";
                        resolve([
                          '${entry.key}',
                          event.target.value,
                          '${entry.value}',
                          eventInterface
                        ]);
                }, { once: true }))""")
            }
        }

        val autoFillValues = SparseArray<CharSequence>()

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: MockViewNode) {
            // Seal the node info instance so we can perform actions on it.
            if (child.childCount > 0) {
                for (c in child.children) {
                    checkAutoFillChild(c!!)
                }
            }

            if (child.id == View.NO_ID) {
                return
            }

            if (Build.VERSION.SDK_INT >= 26) {
                assertThat("Should have HTML tag",
                           child.htmlInfo!!.tag, not(isEmptyOrNullString()))
                assertThat("Web domain should match",
                           child.webDomain, equalTo(GeckoSessionTestRule.TEST_ENDPOINT))
            }

            if (EditText::class.java.name == child.className) {
                assertThat("Input should be enabled", child.isEnabled, equalTo(true))
                assertThat("Input should be focusable",
                           child.isFocusable, equalTo(true))
                assertThat("Input should be visible",
                           child.visibility, equalTo(View.VISIBLE))

                if (Build.VERSION.SDK_INT < 26) {
                    autoFillValues.append(child.id, "bar")
                    return
                }

                val htmlInfo = child.htmlInfo
                assertThat("Should have HTML tag", htmlInfo!!.tag, equalTo("input"))
                assertThat("Should have ID attribute",
                           htmlInfo.attributes!!.map { it.first }, hasItem("id"))

                assertThat("Autofill type should match",
                           child.autofillType, equalTo(View.AUTOFILL_TYPE_TEXT))

                assertThat("Autofill hints should match", child.autofillHints, equalTo(
                        when (child.inputType) {
                            InputType.TYPE_CLASS_TEXT or
                                    InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD ->
                                arrayOf(View.AUTOFILL_HINT_PASSWORD)
                            InputType.TYPE_CLASS_TEXT or
                                    InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS ->
                                arrayOf(View.AUTOFILL_HINT_EMAIL_ADDRESS)
                            InputType.TYPE_CLASS_PHONE -> arrayOf(View.AUTOFILL_HINT_PHONE)
                            InputType.TYPE_CLASS_TEXT or
                                    InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT ->
                                arrayOf(View.AUTOFILL_HINT_USERNAME)
                            else -> null
                        }))

                autoFillValues.append(child.id, when (child.inputType) {
                    InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD -> "baz"
                    InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS -> "a@b.c"
                    InputType.TYPE_CLASS_NUMBER -> "24"
                    InputType.TYPE_CLASS_PHONE -> "42"
                    else -> "bar"
                })
            }
        }

        val rootStructure = MockViewNode()

        mainSession.textInput.onProvideAutofillVirtualStructure(rootStructure, 0)
        checkAutoFillChild(rootStructure)

        mainSession.textInput.autofill(autoFillValues)

        // Wait on the promises and check for correct values.
        for ((key, actual, expected, eventInterface) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match", actual, equalTo(expected))

            // <input type=number> elements don't get InputEvent events.
            if (key == "#number1") {
                assertThat("input type=number event should be dispatched with Event interface", eventInterface, equalTo("Event"))
            } else {
                assertThat("input event should be dispatched with InputEvent interface", eventInterface, equalTo("InputEvent"))
            }
        }
    }

    // TextInputDelegateTest is parameterized, so we put this test under ContentDelegateTest.
    @SdkSuppress(minSdkVersion = 23)
    // The small screen is so that the page is forced to scroll to show the input
    // and firing the autofill event.
    // XXX(agi): This shouldn't be necessary though? What if the page doesn't scroll?
    @WithDisplay(width = 10, height = 10)
    @Test fun autoFill_navigation() {
        fun countAutoFillNodes(cond: (MockViewNode) -> Boolean =
                                       { it.className == "android.widget.EditText" },
                               root: MockViewNode? = null): Int {
            val node = if (root !== null) root else MockViewNode().also {
                mainSession.textInput.onProvideAutofillVirtualStructure(it, 0)
            }

            return (if (cond(node)) 1 else 0) +
                    (if (node.childCount > 0) node.children.sumBy {
                        countAutoFillNodes(cond, it) } else 0)
        }

        // Wait for the accessibility nodes to populate.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 4)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be starting auto-fill", notification, equalTo(forEachCall(
                        GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_STARTED,
                        GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ADDED)))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Initial auto-fill count should match",
                   countAutoFillNodes(), equalTo(14))

        // Now wait for the nodes to clear.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be canceling auto-fill",
                           notification,
                           equalTo(GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_CANCELED))
                assertThat("ID should be valid", virtualId, equalTo(View.NO_ID))
            }
        })
        assertThat("Should not have auto-fill fields",
                   countAutoFillNodes(), equalTo(0))

        // Now wait for the nodes to reappear.
        mainSession.waitForPageStop()
        mainSession.goBack()
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 4)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be starting auto-fill", notification, equalTo(forEachCall(
                        GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_STARTED,
                        GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ADDED)))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have auto-fill fields again",
                   countAutoFillNodes(), equalTo(14))
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(0))

        mainSession.evaluateJS("document.querySelector('#pass2').focus()")
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be entering auto-fill view",
                           notification,
                           equalTo(GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should have one focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(1))
        // The focused field, its siblings, its parent, and the root node should be visible.
        assertThat("Should have seven visible nodes",
                   countAutoFillNodes({ node -> node.width > 0 && node.height > 0 }),
                   equalTo(7))

        mainSession.evaluateJS("document.querySelector('#pass2').blur()")
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
                assertThat("Should be exiting auto-fill view",
                           notification,
                           equalTo(GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_EXITED))
                assertThat("ID should be valid", virtualId, not(equalTo(View.NO_ID)))
            }
        })
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(0))
    }

    @WithDisplay(height = 100, width = 100)
    @Test fun autofill_userpass() {
        if (Build.VERSION.SDK_INT < 26) {
            return
        }

        mainSession.loadTestPath(FORMS2_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
            }
        })

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: MockViewNode): Int {
            var sum = 0
            // Seal the node info instance so we can perform actions on it.
            if (child.children.size > 0) {
                for (c in child.children) {
                    sum += checkAutoFillChild(c!!)
                }
            }

            if (child.id == View.NO_ID) {
                return sum
            }

            assertThat("ID should be valid", child.id, not(equalTo(View.NO_ID)))

            if (EditText::class.java.name == child.className) {
                val htmlInfo = child.htmlInfo!!
                assertThat("Should have HTML tag", htmlInfo.tag, equalTo("input"))

                if (child.autofillHints == null) {
                    return sum
                }
                child.autofillHints!!.forEach {
                    when (it) {
                        View.AUTOFILL_HINT_USERNAME, View.AUTOFILL_HINT_PASSWORD -> {
                            sum++
                        }
                    }
                }
            }
            return sum
        }

        val rootStructure = MockViewNode()

        mainSession.textInput.onProvideAutofillVirtualStructure(rootStructure, 0)
        // form and iframe have each 2 hints.
        assertThat("autofill hint count",
                   checkAutoFillChild(rootStructure), equalTo(4))
    }

    class MockViewNode : ViewStructure() {
        private var mClassName: String? = null
        private var mEnabled = false
        private var mVisibility = -1
        private var mPackageName: String? = null
        private var mTypeName: String? = null
        private var mEntryName: String? = null
        private var mAutofillType = -1
        private var mAutofillHints: Array<String>? = null
        private var mInputType = -1
        private var mHtmlInfo: HtmlInfo? = null
        private var mWebDomain: String? = null
        private var mFocused = false
        private var mFocusable = false

        var children = ArrayList<MockViewNode?>()
        var id = View.NO_ID
        var height = 0
        var width = 0

        val className get() = mClassName
        val htmlInfo get() = mHtmlInfo
        val autofillHints get() = mAutofillHints
        val autofillType get() = mAutofillType
        val webDomain get() = mWebDomain
        val isEnabled get() = mEnabled
        val isFocused get() = mFocused
        val isFocusable get() = mFocusable
        val visibility get() = mVisibility
        val inputType get() = mInputType

        override fun setId(id: Int, packageName: String?, typeName: String?, entryName: String?) {
            this.id = id
            mPackageName = packageName
            mTypeName = typeName
            mEntryName = entryName
        }

        override fun setHint(hint: CharSequence?) {
            TODO("not implemented")
        }

        override fun setElevation(elevation: Float) {
            TODO("not implemented")
        }

        override fun getText(): CharSequence {
            TODO("not implemented")
        }

        override fun setText(text: CharSequence?) {
            TODO("not implemented")
        }

        override fun setText(text: CharSequence?, selectionStart: Int, selectionEnd: Int) {
            TODO("not implemented")
        }

        override fun asyncCommit() {
            TODO("not implemented")
        }

        override fun getChildCount(): Int = children.size

        override fun setEnabled(state: Boolean) {
            mEnabled = state
        }

        override fun setLocaleList(localeList: LocaleList?) {
            TODO("not implemented")
        }

        override fun setDimens(left: Int, top: Int, scrollX: Int, scrollY: Int, width: Int, height: Int) {
            this.width = width
            this.height = height
        }

        override fun setChecked(state: Boolean) {
            TODO("not implemented")
        }

        override fun setContextClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun setAccessibilityFocused(state: Boolean) {
            TODO("not implemented")
        }

        override fun setAlpha(alpha: Float) {
            TODO("not implemented")
        }

        override fun setTransformation(matrix: Matrix?) {
            TODO("not implemented")
        }

        override fun setClassName(className: String?) {
            mClassName = className
        }

        override fun setLongClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun newChild(index: Int): ViewStructure {
            val child = MockViewNode()
            children[index] = child
            return child
        }

        override fun getHint(): CharSequence {
            TODO("not implemented")
        }

        override fun setInputType(inputType: Int) {
            mInputType = inputType
        }

        override fun setWebDomain(domain: String?) {
            mWebDomain = domain
        }

        override fun setAutofillOptions(options: Array<out CharSequence>?) {
            TODO("not implemented")
        }

        override fun setTextStyle(size: Float, fgColor: Int, bgColor: Int, style: Int) {
            TODO("not implemented")
        }

        override fun setVisibility(visibility: Int) {
            mVisibility = visibility
        }

        override fun getAutofillId(): AutofillId? {
            TODO("not implemented")
        }

        override fun setHtmlInfo(htmlInfo: HtmlInfo) {
            mHtmlInfo = htmlInfo
        }

        override fun setTextLines(charOffsets: IntArray?, baselines: IntArray?) {
            TODO("not implemented")
        }

        override fun getExtras(): Bundle {
            TODO("not implemented")
        }

        override fun setClickable(state: Boolean) {
            TODO("not implemented")
        }

        override fun newHtmlInfoBuilder(tagName: String): HtmlInfo.Builder {
            return MockHtmlInfoBuilder(tagName)
        }

        override fun getTextSelectionEnd(): Int {
            TODO("not implemented")
        }

        override fun setAutofillId(id: AutofillId) {
            TODO("not implemented")
        }

        override fun setAutofillId(parentId: AutofillId, virtualId: Int) {
            TODO("not implemented")
        }

        override fun hasExtras(): Boolean {
            TODO("not implemented")
        }

        override fun addChildCount(num: Int): Int {
            TODO("not implemented")
        }

        override fun setAutofillType(type: Int) {
            mAutofillType = type
        }

        override fun setActivated(state: Boolean) {
            TODO("not implemented")
        }

        override fun setFocused(state: Boolean) {
            mFocused = state
        }

        override fun getTextSelectionStart(): Int {
            TODO("not implemented")
        }

        override fun setChildCount(num: Int) {
            children = ArrayList()
            for (i in 0 until num) {
                children.add(null)
            }
        }

        override fun setAutofillValue(value: AutofillValue?) {
            TODO("not implemented")
        }

        override fun setAutofillHints(hint: Array<String>?) {
            mAutofillHints = hint
        }

        override fun setContentDescription(contentDescription: CharSequence?) {
            TODO("not implemented")
        }

        override fun setFocusable(state: Boolean) {
            mFocusable = state
        }

        override fun setCheckable(state: Boolean) {
            TODO("not implemented")
        }

        override fun asyncNewChild(index: Int): ViewStructure {
            TODO("not implemented")
        }

        override fun setSelected(state: Boolean) {
            TODO("not implemented")
        }

        override fun setDataIsSensitive(sensitive: Boolean) {
            TODO("not implemented")
        }

        override fun setOpaque(opaque: Boolean) {
            TODO("not implemented")
        }
    }

    class MockHtmlInfoBuilder(tagName: String) : ViewStructure.HtmlInfo.Builder() {
        val mTagName = tagName
        val mAttributes: MutableList<Pair<String, String>> = mutableListOf()

        override fun addAttribute(name: String, value: String): ViewStructure.HtmlInfo.Builder {
            mAttributes.add(Pair(name, value))
            return this
        }

        override fun build(): ViewStructure.HtmlInfo {
            return MockHtmlInfo(mTagName, mAttributes)
        }
    }

    class MockHtmlInfo(tagName: String, attributes: MutableList<Pair<String, String>>)
            : ViewStructure.HtmlInfo() {
        private val mTagName = tagName
        private val mAttributes = attributes

        override fun getTag() = mTagName
        override fun getAttributes() = mAttributes
    }

    private fun goFullscreen() {
        sessionRule.setPrefsUntilTestEnd(mapOf("full-screen-api.allow-trusted-requests-only" to false))
        mainSession.loadTestPath(FULLSCREEN_PATH)
        mainSession.waitForPageStop()
        mainSession.evaluateJS("document.querySelector('#fullscreen').requestFullscreen(); true")
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            override  fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                assertThat("Div went fullscreen", fullScreen, equalTo(true))
            }
        })
    }

    private fun waitForFullscreenExit() {
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            override  fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                assertThat("Div went fullscreen", fullScreen, equalTo(false))
            }
        })
    }

    @Test fun fullscreen() {
        goFullscreen()
        mainSession.evaluateJS("document.exitFullscreen(); true")
        waitForFullscreenExit()
    }

    @Test fun sessionExitFullscreen() {
        goFullscreen()
        mainSession.exitFullScreen()
        waitForFullscreenExit()
    }

    @Test fun firstComposite() {
        val display = mainSession.acquireDisplay()
        val texture = SurfaceTexture(0)
        texture.setDefaultBufferSize(100, 100)
        val surface = Surface(texture)
        display.surfaceChanged(surface, 100, 100)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstComposite(session: GeckoSession) {
            }
        })
        display.surfaceDestroyed()
        display.surfaceChanged(surface, 100, 100)
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstComposite(session: GeckoSession) {
            }
        })
        display.surfaceDestroyed()
        mainSession.releaseDisplay(display)
    }

    @Test fun webAppManifest() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitUntilCalled(object : Callbacks.All {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page load should succeed", success, equalTo(true))
            }

            @AssertCalled(count = 1)
            override fun onWebAppManifest(session: GeckoSession, manifest: JSONObject) {
                // These values come from the manifest at assets/www/manifest.webmanifest
                assertThat("name should match", manifest.getString("name"), equalTo("App"))
                assertThat("short_name should match", manifest.getString("short_name"), equalTo("app"))
                assertThat("display should match", manifest.getString("display"), equalTo("standalone"))

                // The color here is "cadetblue" converted to hex.
                assertThat("theme_color should match", manifest.getString("theme_color"), equalTo("#5f9ea0"))
                assertThat("background_color should match", manifest.getString("background_color"), equalTo("#c0feee"))
                assertThat("start_url should match", manifest.getString("start_url"), endsWith("/assets/www/start/index.html"))

                val icon = manifest.getJSONArray("icons").getJSONObject(0);

                val iconSrc = Uri.parse(icon.getString("src"))
                assertThat("icon should have a valid src", iconSrc, notNullValue())
                assertThat("icon src should be absolute", iconSrc.isAbsolute, equalTo(true))
                assertThat("icon should have sizes", icon.getString("sizes"),  not(isEmptyOrNullString()))
                assertThat("icon type should match", icon.getString("type"), equalTo("image/gif"))
            }
        })
    }


    /**
     * Preferences to induce wanted behaviour.
     */
    private fun setHangReportTestPrefs(timeout: Int = 20000) {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "dom.max_script_run_time" to 1,
                "dom.max_chrome_script_run_time" to 1,
                "dom.max_ext_content_script_run_time" to 1,
                "dom.ipc.cpow.timeout" to 100,
                "browser.hangNotification.waitPeriod" to timeout
        ))
    }

    /**
     * With no delegate set, the default behaviour is to stop hung scripts.
     */
    @NullDelegate(GeckoSession.ContentDelegate::class)
    @Test fun stopHungProcessDefault() {
        setHangReportTestPrefs()
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The script did not complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Started"))
            }
        })
        sessionRule.waitForPageStop(mainSession)
    }

    /**
     * With no overriding implementation for onSlowScript, the default behaviour is to stop hung
     * scripts.
     */
    @Test fun stopHungProcessNull() {
        setHangReportTestPrefs()
        sessionRule.delegateUntilTestEnd(object : GeckoSession.ContentDelegate, Callbacks.ProgressDelegate {
            // default onSlowScript returns null
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The script did not complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Started"))
            }
        })
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.waitForPageStop(mainSession)
    }

    /**
     * Test that, with a 'do nothing' delegate, the hung process completes after its delay
     */
    @Test fun stopHungProcessDoNothing() {
        setHangReportTestPrefs()
        var scriptHungReportCount = 0
        sessionRule.delegateUntilTestEnd(object : GeckoSession.ContentDelegate, Callbacks.ProgressDelegate {
            @AssertCalled()
            override fun onSlowScript(geckoSession: GeckoSession, scriptFileName: String): GeckoResult<SlowScriptResponse> {
                scriptHungReportCount += 1;
                return GeckoResult.fromValue(null)
            }
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The delegate was informed of the hang repeatedly", scriptHungReportCount, greaterThan(1))
                assertThat("The script did complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Finished"))
            }
        })
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.waitForPageStop(mainSession)
    }

    /**
     * Test that the delegate is called and can stop a hung script
     */
    @Test fun stopHungProcess() {
        setHangReportTestPrefs()
        sessionRule.delegateUntilTestEnd(object : GeckoSession.ContentDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onSlowScript(geckoSession: GeckoSession, scriptFileName: String): GeckoResult<SlowScriptResponse> {
                return GeckoResult.fromValue(SlowScriptResponse.STOP)
            }
            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The script did not complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Started"))
            }
        })
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.waitForPageStop(mainSession)
    }

    /**
     * Test that the delegate is called and can continue executing hung scripts
     */
    @Test fun stopHungProcessWait() {
        setHangReportTestPrefs()
        sessionRule.delegateUntilTestEnd(object : GeckoSession.ContentDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onSlowScript(geckoSession: GeckoSession, scriptFileName: String): GeckoResult<SlowScriptResponse> {
                return GeckoResult.fromValue(SlowScriptResponse.CONTINUE)
            }
            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The script did complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Finished"))
            }
        })
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.waitForPageStop(mainSession)
    }

    /**
     * Test that the delegate is called and paused scripts re-notify after the wait period
     */
    @Test fun stopHungProcessWaitThenStop() {
        setHangReportTestPrefs(500)
        var scriptWaited = false
        sessionRule.delegateUntilTestEnd(object : GeckoSession.ContentDelegate, Callbacks.ProgressDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onSlowScript(geckoSession: GeckoSession, scriptFileName: String): GeckoResult<SlowScriptResponse> {
                return if (!scriptWaited) {
                    scriptWaited = true;
                    GeckoResult.fromValue(SlowScriptResponse.CONTINUE)
                } else {
                    GeckoResult.fromValue(SlowScriptResponse.STOP)
                }
            }
            @AssertCalled(count = 1, order = [3])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("The script did not complete.",
                        sessionRule.session.evaluateJS("document.getElementById(\"content\").innerHTML") as String,
                        equalTo("Started"))
            }
        })
        mainSession.loadTestPath(HUNG_SCRIPT)
        sessionRule.waitForPageStop(mainSession)
    }
}
