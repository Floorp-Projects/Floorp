package org.mozilla.geckoview.test

import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
@WithDevToolsAPI
class PromptDelegateTest : BaseSessionTest() {
    @Test fun popupTest() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))
        sessionRule.session.loadTestPath(POPUP_HTML_PATH)

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onPopupRequest(session: GeckoSession, targetUri: String): GeckoResult<AllowOrDeny>? {
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
            override fun onPopupRequest(session: GeckoSession, targetUri: String): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", targetUri, notNullValue())
                assertThat("URL should match", targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String, where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", uri, notNullValue())
                assertThat("URL should match", uri, endsWith(forEachCall(POPUP_HTML_PATH, HELLO_HTML_PATH)))
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
            override fun onPopupRequest(session: GeckoSession, targetUri: String): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", targetUri, notNullValue())
                assertThat("URL should match", targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(AllowOrDeny.DENY)
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(session: GeckoSession, uri: String, where: Int, flags: Int): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", uri, notNullValue())
                assertThat("URL should match", uri, endsWith(POPUP_HTML_PATH))
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
}
