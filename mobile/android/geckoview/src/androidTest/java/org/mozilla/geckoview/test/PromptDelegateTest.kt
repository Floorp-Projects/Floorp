package org.mozilla.geckoview.test

import android.support.test.InstrumentationRegistry
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.util.HttpBin
import java.net.URI

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
@WithDevToolsAPI
class PromptDelegateTest : BaseSessionTest() {
    companion object {
        val TEST_ENDPOINT: String = "http://localhost:4243"
    }

    @Ignore("disable test for frequently failing Bug 1535423")
    @Test fun popupTest() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))
        sessionRule.session.loadTestPath(POPUP_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onPopupRequest(session: GeckoSession, targetUri: String?)
                    : GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", targetUri, notNullValue())
                assertThat("URL should match", targetUri, endsWith(HELLO_HTML_PATH))
                return null
            }
        })
    }

    @Test fun popupTestAllow() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate, Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupRequest(session: GeckoSession, targetUri: String?)
                    : GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", targetUri, notNullValue())
                assertThat("URL should match", targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", request.uri, notNullValue())
                assertThat("URL should match", request.uri, endsWith(forEachCall(POPUP_HTML_PATH, HELLO_HTML_PATH)))
                return null
            }
        })

        sessionRule.session.loadTestPath(POPUP_HTML_PATH)
        sessionRule.waitUntilCalled(Callbacks.NavigationDelegate::class, "onNewSession")
    }

    @Test fun popupTestBlock() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate, Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupRequest(session: GeckoSession, targetUri: String?)
                    : GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", targetUri, notNullValue())
                assertThat("URL should match", targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(AllowOrDeny.DENY)
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", request.uri, notNullValue())
                assertThat("URL should match", request.uri, endsWith(POPUP_HTML_PATH))
                return null
            }

            @AssertCalled(count = 0)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                return null
            }
        })

        sessionRule.session.loadTestPath(POPUP_HTML_PATH)
        sessionRule.session.waitForPageStop()
    }

    @Ignore // TODO: Reenable when 1501574 is fixed.
    @Test fun alertTest() {
        sessionRule.session.evaluateJS("alert('Alert!');")

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onAlert(session: GeckoSession, title: String?, msg: String?, callback: GeckoSession.PromptDelegate.AlertCallback) {
                assertThat("Message should match", "Alert!", equalTo(msg))
            }
        })
    }

    @Test fun authTest() {
        val httpBin = HttpBin(InstrumentationRegistry.getTargetContext(), URI.create(TEST_ENDPOINT))

        try {
            httpBin.start()

            sessionRule.session.loadUri("$TEST_ENDPOINT/basic-auth/foo/bar")

            sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
                @AssertCalled(count = 1)
                override fun onAuthPrompt(session: GeckoSession, title: String?, msg: String?, options: GeckoSession.PromptDelegate.AuthOptions, callback: GeckoSession.PromptDelegate.AuthCallback) {
                    //TODO: Figure out some better testing here.
                }
            })
        } finally {
            httpBin.stop()
        }
    }

    @Ignore // TODO: Reenable when 1501574 is fixed.
    @Test fun buttonTest() {
        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, title: String?, msg: String?, btnMsg: Array<out String>?, callback: GeckoSession.PromptDelegate.ButtonCallback) {
                assertThat("Message should match", "Confirm?", equalTo(msg))
                callback.confirm(GeckoSession.PromptDelegate.BUTTON_TYPE_POSITIVE)
            }
        })

        assertThat("Result should match",
                sessionRule.session.waitForJS("confirm('Confirm?')") as Boolean,
                equalTo(true))

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, title: String?, msg: String?, btnMsg: Array<out String>?, callback: GeckoSession.PromptDelegate.ButtonCallback) {
                assertThat("Message should match", "Confirm?", equalTo(msg))
                callback.confirm(GeckoSession.PromptDelegate.BUTTON_TYPE_NEGATIVE)
            }
        })

        assertThat("Result should match",
                sessionRule.session.waitForJS("confirm('Confirm?')") as Boolean,
                equalTo(false))
    }

    @Test fun textTest() {
        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onTextPrompt(session: GeckoSession, title: String?, msg: String?, value: String?, callback: GeckoSession.PromptDelegate.TextCallback) {
                assertThat("Message should match", "Prompt:", equalTo(msg))
                assertThat("Default should match", "default", equalTo(value))
                callback.confirm("foo")
            }
        })

        assertThat("Result should match",
                sessionRule.session.waitForJS("prompt('Prompt:', 'default')") as String,
                equalTo("foo"))
    }

    @Ignore // TODO: Figure out weird test env behavior here.
    @Test fun choiceTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.getElementById('selectexample').click();")

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, title: String?, msg: String?, type: Int, choices: Array<out GeckoSession.PromptDelegate.Choice>, callback: GeckoSession.PromptDelegate.ChoiceCallback) {
            }
        })
    }

    @Test fun colorTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onColorPrompt(session: GeckoSession, title: String?, value: String?, callback: GeckoSession.PromptDelegate.TextCallback) {
                assertThat("Value should match", "#ffffff", equalTo(value))
                callback.confirm("#123456")
            }
        })

        sessionRule.session.evaluateJS("""
            let c = document.getElementById('colorexample');
            let p = new Promise((resolve, reject) => {
                c.addEventListener('change', (event) => resolve(event.target.value), false);
            });
            c.click();""".trimIndent())

        assertThat("Value should match",
                sessionRule.session.waitForJS("p") as String,
                equalTo("#123456"))
    }

    @Ignore // TODO: Figure out weird test env behavior here.
    @Test fun dateTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.getElementById('dateexample').click();")

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, title: String?, type: Int, value: String?, min: String?, max: String?, callback: GeckoSession.PromptDelegate.TextCallback) {
            }
        })
    }

    @Test fun fileTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.getElementById('fileexample').click();")

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onFilePrompt(session: GeckoSession, title: String?, type: Int, mimeTypes: Array<out String>?, callback: GeckoSession.PromptDelegate.FileCallback) {
                assertThat("Length of mimeTypes should match", 2, equalTo(mimeTypes!!.size))
                assertThat("First accept attribute should match", "image/*", equalTo(mimeTypes[0]))
                assertThat("Second accept attribute should match", ".pdf", equalTo(mimeTypes[1]))
                // TODO: Test capture attribute when implemented.
            }
        })
    }
}
