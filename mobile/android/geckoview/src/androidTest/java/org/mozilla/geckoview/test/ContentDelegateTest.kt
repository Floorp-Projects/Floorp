/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.app.assist.AssistStructure
import android.os.Build
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.UiThreadUtils

import android.os.Looper
import android.support.test.filters.MediumTest
import android.support.test.filters.SdkSuppress
import android.support.test.runner.AndroidJUnit4
import android.text.InputType
import android.util.SparseArray
import android.view.View
import android.view.ViewStructure
import android.widget.EditText
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith

import kotlin.concurrent.thread

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateTest : BaseSessionTest() {

    @Test fun titleChange() {
        sessionRule.session.loadTestPath(TITLE_CHANGE_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 2)
            override fun onTitleChange(session: GeckoSession, title: String) {
                assertThat("Title should match", title,
                           equalTo(forEachCall("Title1", "Title2")))
            }
        })
    }

    @Test fun download() {
        sessionRule.session.loadTestPath(DOWNLOAD_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate, Callbacks.ContentDelegate {

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
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
    @ReuseSession(false)
    @Test fun crashContent() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.cpuArch == "x86",
                   equalTo(false))

        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onCrash(session: GeckoSession) {
                assertThat("Session should be closed after a crash",
                           session.isOpen, equalTo(false))
            }
        });

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
    @ReuseSession(false)
    @WithDisplay(width = 10, height = 10)
    @Test fun crashContent_tapAfterCrash() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.cpuArch == "x86",
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
    @ReuseSession(false)
    @Test fun crashContentMultipleSessions() {
        // This test doesn't make sense without multiprocess
        assumeThat(sessionRule.env.isMultiprocess, equalTo(true))
        // Cannot test x86 debug builds due to Gecko's "ah_crap_handler"
        // that waits for debugger to attach during a SIGSEGV.
        assumeThat(sessionRule.env.isDebugBuild && sessionRule.env.cpuArch == "x86",
                   equalTo(false))

        // XXX we need to make sure all sessions in a given content process receive onCrash().
        // If we add multiple content processes, this test will need to be fixed to ensure the
        // test sessions go into the same one.
        val newSession = sessionRule.createOpenSession()
        mainSession.loadUri(CONTENT_CRASH_URL)

        // We can inadvertently catch the `onCrash` call for the cached session if we don't specify
        // individual sessions here. Therefore, assert 'onCrash' is called for the two sessions
        // individually.
        val remainingSessions = mutableListOf(newSession, mainSession)
        while (remainingSessions.isNotEmpty()) {
            sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
                @AssertCalled(count = 1)
                override fun onCrash(session: GeckoSession) {
                    remainingSessions.remove(session)
                }
            })
        }
    }

    @WithDevToolsAPI
    @WithDisplay(width = 400, height = 400)
    @Test fun saveAndRestoreState() {
        val startUri = createTestUrl(SAVE_STATE_PATH)
        mainSession.loadUri(startUri)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS("$('#name').value = 'the name'; window.scrollBy(0, 100);")

        val state = sessionRule.waitForResult(mainSession.saveState())
        assertThat("State should not be null", state, notNullValue())

        mainSession.loadUri("about:blank")
        sessionRule.waitForPageStop()

        mainSession.restoreState(state)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URI should match", url, equalTo(startUri))
            }
        })

        assertThat("'name' field should match",
                mainSession.evaluateJS("$('#name').value").toString(),
                equalTo("the name"))

        assertThat("Scroll position should match",
                mainSession.evaluateJS("window.scrollY") as Double,
                closeTo(100.0, .5))
    }

    @Test fun saveStateSync() {
        val startUri = createTestUrl(SAVE_STATE_PATH)
        mainSession.loadUri(startUri)
        sessionRule.waitForPageStop()

        var worker = thread {
            Looper.prepare()

            var thread = Thread.currentThread()
            mainSession.saveState().then<Void> { _: GeckoSession.SessionState? ->
                assertThat("We should be on the worker thread", Thread.currentThread(),
                        equalTo(thread))
                Looper.myLooper().quit()
                null
            }

            Looper.loop()
        }

        worker.join(sessionRule.timeoutMillis)
        if (worker.isAlive) {
            throw UiThreadUtils.TimeoutException("Timed out")
        }
    }

    val ViewNode by lazy {
        AssistStructure.ViewNode::class.java.getDeclaredConstructor().apply { isAccessible = true }
    }

    val ViewNodeBuilder by lazy {
        Class.forName("android.app.assist.AssistStructure\$ViewNodeBuilder")
                .getDeclaredConstructor(AssistStructure::class.java,
                                        AssistStructure.ViewNode::class.java,
                                        Boolean::class.javaPrimitiveType)
                .apply { isAccessible = true }
    }

    // TextInputDelegateTest is parameterized, so we put this test under ContentDelegateTest.
    @SdkSuppress(minSdkVersion = 23)
    @WithDevToolsAPI
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
            arrayOf("document", "$('#iframe').contentDocument").map { doc ->
                mainSession.evaluateJS("""new Promise(resolve =>
                $doc.querySelector('${entry.key}').addEventListener(
                    'input', event => resolve([event.target.value, '${entry.value}']),
                    { once: true }))""").asJSPromise()
            }
        }

        val rootNode = ViewNode.newInstance()
        val rootStructure = ViewNodeBuilder.newInstance(AssistStructure(), rootNode,
                /* async */ false) as ViewStructure
        val autoFillValues = SparseArray<CharSequence>()

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: AssistStructure.ViewNode) {
            // Seal the node info instance so we can perform actions on it.
            if (child.childCount > 0) {
                for (i in 0 until child.childCount) {
                    checkAutoFillChild(child.getChildAt(i))
                }
            }

            if (child === rootNode) {
                return
            }

            assertThat("ID should be valid", child.id, not(equalTo(View.NO_ID)))

            if (Build.VERSION.SDK_INT >= 26) {
                assertThat("Should have HTML tag",
                           child.htmlInfo.tag, not(isEmptyOrNullString()))
                assertThat("Web domain should match",
                           child.webDomain, equalTo("android"))
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
                assertThat("Should have HTML tag", htmlInfo.tag, equalTo("input"))
                assertThat("Should have ID attribute",
                           htmlInfo.attributes.map { it.first }, hasItem("id"))

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

        mainSession.textInput.onProvideAutofillVirtualStructure(rootStructure, 0)
        checkAutoFillChild(rootNode)
        mainSession.textInput.autofill(autoFillValues)

        // Wait on the promises and check for correct values.
        for ((actual, expected) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match", actual, equalTo(expected))
        }
    }

    // TextInputDelegateTest is parameterized, so we put this test under ContentDelegateTest.
    @SdkSuppress(minSdkVersion = 23)
    @WithDevToolsAPI
    @WithDisplay(width = 100, height = 100)
    @Test fun autoFill_navigation() {

        fun countAutoFillNodes(cond: (AssistStructure.ViewNode) -> Boolean =
                                       { it.className == "android.widget.EditText" },
                               root: AssistStructure.ViewNode? = null): Int {
            val node = if (root !== null) root else ViewNode.newInstance().also {
                // Fill the nodes first.
                val structure = ViewNodeBuilder.newInstance(
                        AssistStructure(), it, /* async */ false) as ViewStructure
                mainSession.textInput.onProvideAutofillVirtualStructure(structure, 0)
            }
            return (if (cond(node)) 1 else 0) +
                    (if (node.childCount > 0) (0 until node.childCount).sumBy {
                        countAutoFillNodes(cond, node.getChildAt(it)) } else 0)
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

        mainSession.evaluateJS("$('#pass1').focus()")
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

        mainSession.evaluateJS("$('#pass1').blur()")
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
}
