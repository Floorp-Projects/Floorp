/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.app.assist.AssistStructure
import android.graphics.SurfaceTexture
import android.net.Uri
import android.os.Build
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.UiThreadUtils

import android.os.Looper
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest
import android.support.test.filters.SdkSuppress
import android.support.test.runner.AndroidJUnit4
import android.text.InputType
import android.util.SparseArray
import android.view.Surface
import android.view.View
import android.view.ViewStructure
import android.widget.EditText
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.util.HttpBin

import java.net.URI

import kotlin.concurrent.thread

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateTest : BaseSessionTest() {
    companion object {
        val TEST_ENDPOINT: String = "http://localhost:4243"
    }

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
        sessionRule.session.loadTestPath(DOWNLOAD_HTML_PATH)
        // disable test on pgo for frequently failing Bug 1543355
        assumeThat(sessionRule.env.isDebugBuild, equalTo(true))

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
    @ReuseSession(false)
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
    @ReuseSession(false)
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
                    'input', event => {
                      let eventInterface =
                        event instanceof InputEvent ? "InputEvent" :
                        event instanceof UIEvent ? "UIEvent" :
                        event instanceof Event ? "Event" : "Unknown";
                      resolve([event.target.value, '${entry.value}', eventInterface]);
                    }, { once: true }))""").asJSPromise()
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

        mainSession.textInput.onProvideAutofillVirtualStructure(rootStructure, 0)
        checkAutoFillChild(rootNode)
        mainSession.textInput.autofill(autoFillValues)

        // Wait on the promises and check for correct values.
        for ((actual, expected, eventInterface) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match", actual, equalTo(expected))
            assertThat("input event should be dispatched with InputEvent interface", eventInterface, equalTo("InputEvent"))
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

    @WithDisplay(height = 100, width = 100)
    @WithDevToolsAPI
    @Test fun autofill_userpass() {
        if (Build.VERSION.SDK_INT < 26) {
            return
        }

        mainSession.loadTestPath(FORMS2_HTML_PATH)
        // Wait for the auto-fill nodes to populate.
        sessionRule.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 2)
            override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
            }
        })

        val rootNode = ViewNode.newInstance()
        val rootStructure = ViewNodeBuilder.newInstance(AssistStructure(), rootNode,
                /* async */ false) as ViewStructure

        // Perform auto-fill and return number of auto-fills performed.
        fun checkAutoFillChild(child: AssistStructure.ViewNode): Int {
            var sum = 0
            // Seal the node info instance so we can perform actions on it.
            if (child.childCount > 0) {
                for (i in 0 until child.childCount) {
                    sum += checkAutoFillChild(child.getChildAt(i))
                }
            }

            if (child === rootNode) {
                return sum
            }

            assertThat("ID should be valid", child.id, not(equalTo(View.NO_ID)))

            if (EditText::class.java.name == child.className) {
                val htmlInfo = child.htmlInfo
                assertThat("Should have HTML tag", htmlInfo.tag, equalTo("input"))

                if (child.autofillHints == null) {
                    return sum
                }
                child.autofillHints.forEach {
                    when (it) {
                        View.AUTOFILL_HINT_USERNAME, View.AUTOFILL_HINT_PASSWORD -> {
                            sum++
                        }
                    }
                }
            }
            return sum
        }

        mainSession.textInput.onProvideAutofillVirtualStructure(rootStructure, 0)
        // form and iframe have each 2 hints.
        assertThat("autofill hint count",
                   checkAutoFillChild(rootNode), equalTo(4))
    }

    private fun goFullscreen() {
        sessionRule.setPrefsUntilTestEnd(mapOf("full-screen-api.allow-trusted-requests-only" to false))
        mainSession.loadTestPath(FULLSCREEN_PATH)
        mainSession.waitForPageStop()
        mainSession.evaluateJS("$('#fullscreen').requestFullscreen()")
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

    @WithDevToolsAPI
    @Test fun fullscreen() {
        goFullscreen()
        mainSession.evaluateJS("document.exitFullscreen()")
        waitForFullscreenExit()
    }

    @WithDevToolsAPI
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
        val httpBin = HttpBin(InstrumentationRegistry.getTargetContext(), URI.create(TEST_ENDPOINT))

        try {
            httpBin.start()

            mainSession.loadUri("$TEST_ENDPOINT$HELLO_HTML_PATH")
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
                    assertThat("start_url should match", manifest.getString("start_url"), equalTo("$TEST_ENDPOINT/assets/www/start/index.html"))

                    val icon = manifest.getJSONArray("icons").getJSONObject(0);

                    val iconSrc = Uri.parse(icon.getString("src"))
                    assertThat("icon should have a valid src", iconSrc, notNullValue())
                    assertThat("icon src should be absolute", iconSrc.isAbsolute, equalTo(true))
                    assertThat("icon should have sizes", icon.getString("sizes"),  not(isEmptyOrNullString()))
                    assertThat("icon type should match", icon.getString("type"), equalTo("image/gif"))
                }
            })
        } finally {
            httpBin.stop()
        }
    }
}
