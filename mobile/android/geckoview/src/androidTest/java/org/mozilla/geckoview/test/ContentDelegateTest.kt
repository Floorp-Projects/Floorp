/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.SurfaceTexture
import android.net.Uri
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.IgnoreCrash
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import androidx.annotation.AnyThread
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.view.Surface
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
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

    @Test fun downloadOneRequest() {
        // disable test on pgo for frequently failing Bug 1543355
        assumeThat(sessionRule.env.isDebugBuild, equalTo(true))

        sessionRule.session.loadTestPath(DOWNLOAD_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.NavigationDelegate, Callbacks.ContentDelegate {

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, request: LoadRequest): GeckoResult<AllowOrDeny>? {
                return null
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }

            @AssertCalled(count = 1)
            override fun onExternalResponse(session: GeckoSession, response: WebResponse) {
                assertThat("Uri should start with data:", response.uri, startsWith("blob:"))
                assertThat("We should download the thing", String(response.body?.readBytes()!!), equalTo("Downloaded Data"))
                // The headers below are special headers that we try to get for responses of any kind (http, blob, etc.)
                // Note the case of the header keys. In the WebResponse object, all of them are lower case.
                assertThat("Content type should match", response.headers.get("content-type"), equalTo("text/plain"))
                assertThat("Content length should be non-zero", response.headers.get("Content-Length")!!.toLong(), greaterThan(0L))
                assertThat("Filename should match", response.headers.get("cONTent-diSPOsiTion"), equalTo("attachment; filename=\"download.txt\""))
            }
        })
    }

    @IgnoreCrash
    @Test fun crashContent() {
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

    @AnyThread
    fun killAllContentProcesses() {
        val contentProcessPids = sessionRule.getAllSessionPids()
        for (pid in contentProcessPids) {
            sessionRule.killContentProcess(pid)
        }
    }

    @IgnoreCrash
    @Test fun killContent() {
        killAllContentProcesses()
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

    private fun goFullscreen() {
        sessionRule.setPrefsUntilTestEnd(mapOf("full-screen-api.allow-trusted-requests-only" to false))
        mainSession.loadTestPath(FULLSCREEN_PATH)
        mainSession.waitForPageStop()
        val promise = mainSession.evaluatePromiseJS("document.querySelector('#fullscreen').requestFullscreen()")
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override  fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                assertThat("Div went fullscreen", fullScreen, equalTo(true))
            }
        })
        promise.value
    }

    private fun waitForFullscreenExit() {
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override  fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                assertThat("Div left fullscreen", fullScreen, equalTo(false))
            }
        })
    }

    @Test fun fullscreen() {
        goFullscreen()
        val promise = mainSession.evaluatePromiseJS("document.exitFullscreen()")
        waitForFullscreenExit()
        promise.value
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

    @WithDisplay(width = 10, height = 10)
    @Test fun firstContentfulPaint() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })
    }

    @Test fun webAppManifestPref() {
        val initialState = sessionRule.runtime.settings.getWebManifestEnabled()
        val jsToRun = "document.querySelector('link[rel=manifest]').relList.supports('manifest');"

        // Check pref'ed off
        sessionRule.runtime.settings.setWebManifestEnabled(false)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop(mainSession)

        var result = equalTo(mainSession.evaluateJS(jsToRun) as Boolean)

        assertThat("Disabling pref makes relList.supports('manifest') return false", false, result)

        // Check pref'ed on
        sessionRule.runtime.settings.setWebManifestEnabled(true)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop(mainSession)

        result = equalTo(mainSession.evaluateJS(jsToRun) as Boolean)
        assertThat("Enabling pref makes relList.supports('manifest') return true", true, result)

        sessionRule.runtime.settings.setWebManifestEnabled(initialState)
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

                // The color here is "cadetblue" converted to #aarrggbb.
                assertThat("theme_color should match", manifest.getString("theme_color"), equalTo("#ff5f9ea0"))
                assertThat("background_color should match", manifest.getString("background_color"), equalTo("#eec0ffee"))
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

    @Test fun viewportFit() {
        mainSession.loadTestPath(VIEWPORT_PATH)
        mainSession.waitUntilCalled(object : Callbacks.All {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page load should succeed", success, equalTo(true))
            }

            @AssertCalled(count = 1)
            override fun onMetaViewportFitChange(session: GeckoSession, viewportFit: String) {
                assertThat("viewport-fit should match", viewportFit, equalTo("cover"))
            }
        })

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitUntilCalled(object : Callbacks.All {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page load should succeed", success, equalTo(true))
            }

            @AssertCalled(count = 1)
            override fun onMetaViewportFitChange(session: GeckoSession, viewportFit: String) {
                assertThat("viewport-fit should match", viewportFit, equalTo("auto"))
            }
        })
    }

    @Test fun closeRequest() {
        if (!sessionRule.env.isAutomation) {
            sessionRule.setPrefsUntilTestEnd(mapOf("dom.allow_scripts_to_close_windows" to true))
        }

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("window.close()")
        mainSession.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onCloseRequest(session: GeckoSession) {
            }
        })
    }

    @Test fun windowOpenClose() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val newSession = sessionRule.createClosedSession()
        mainSession.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return GeckoResult.fromValue(newSession)
            }
        })

        mainSession.evaluateJS("const w = window.open('about:blank'); w.close()")

        newSession.waitUntilCalled(object : Callbacks.All {
            @AssertCalled(count = 1)
            override fun onCloseRequest(session: GeckoSession) {
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
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
