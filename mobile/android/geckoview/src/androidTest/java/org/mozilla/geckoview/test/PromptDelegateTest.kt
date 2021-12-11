package org.mozilla.geckoview.test

import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthPrompt
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptResponse
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assert
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autocomplete

@RunWith(AndroidJUnit4::class)
@MediumTest
class PromptDelegateTest : BaseSessionTest() {
    @Test fun popupTestAllow() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateDuringNextWait(object : PromptDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupPrompt(session: GeckoSession, prompt: PromptDelegate.PopupPrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", prompt.targetUri, notNullValue())
                assertThat("URL should match", prompt.targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(prompt.confirm(AllowOrDeny.ALLOW))
            }

            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession,
                                       request: LoadRequest): GeckoResult<AllowOrDeny>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", request.uri, notNullValue())
                assertThat("URL should match", request.uri, endsWith(forEachCall(POPUP_HTML_PATH, HELLO_HTML_PATH)))
                return null
            }

            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
                assertThat("URL should not be null", uri, notNullValue())
                assertThat("URL should match", uri, endsWith(HELLO_HTML_PATH))
                return null
            }
        })

        sessionRule.session.loadTestPath(POPUP_HTML_PATH)
        sessionRule.waitUntilCalled(NavigationDelegate::class, "onNewSession")
    }

    @Test fun popupTestBlock() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateUntilTestEnd(object : PromptDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupPrompt(session: GeckoSession, prompt: PromptDelegate.PopupPrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", prompt.targetUri, notNullValue())
                assertThat("URL should match", prompt.targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(prompt.confirm(AllowOrDeny.DENY))
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
        sessionRule.waitForPageStop()
        sessionRule.session.waitForRoundTrip()
    }

    @Ignore // TODO: Reenable when 1501574 is fixed.
    @Test fun alertTest() {
        sessionRule.session.evaluateJS("alert('Alert!');")

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onAlertPrompt(session: GeckoSession, prompt: PromptDelegate.AlertPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Alert!", equalTo(prompt.message))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    // This test checks that saved logins are returned to the app when calling onAuthPrompt
    @Test fun loginStorageHttpAuthWithPassword() {
        mainSession.loadTestPath("/basic-auth/foo/bar")
        sessionRule.delegateDuringNextWait(object : Autocomplete.StorageDelegate {
            @AssertCalled
            override fun onLoginFetch(domain: String): GeckoResult<Array<Autocomplete.LoginEntry>>? {
                return GeckoResult.fromValue(arrayOf(
                    Autocomplete.LoginEntry.Builder()
                        .origin(GeckoSessionTestRule.TEST_ENDPOINT)
                        .formActionOrigin(GeckoSessionTestRule.TEST_ENDPOINT)
                        .httpRealm("Fake Realm")
                        .username("test-username")
                        .password("test-password")
                        .formActionOrigin(null)
                        .guid("test-guid")
                        .build()
                ));
            }
        })
        sessionRule.waitUntilCalled(object : PromptDelegate, Autocomplete.StorageDelegate {
            @AssertCalled
            override fun onAuthPrompt(session: GeckoSession, prompt: AuthPrompt): GeckoResult<PromptResponse>? {
                assertThat("Saved login should appear here",
                    prompt.authOptions.username, equalTo("test-username"))
                assertThat("Saved login should appear here",
                    prompt.authOptions.password, equalTo("test-password"))
                return null
            }
        })
    }

    // This test checks that we store login information submitted through HTTP basic auth
    // This also tests that the login save prompt gets automatically dismissed if
    // the login information is incorrect.
    @Test fun loginStorageHttpAuth() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
            "signon.rememberSignons" to true))
        val result = GeckoResult<PromptDelegate.BasePrompt>()
        val promptInstanceDelegate = object : PromptDelegate.PromptInstanceDelegate {
            var prompt: PromptDelegate.BasePrompt? = null
            override fun onPromptDismiss(prompt: PromptDelegate.BasePrompt) {
                result.complete(prompt)
            }
        }

        sessionRule.delegateUntilTestEnd(object : PromptDelegate, Autocomplete.StorageDelegate {
            @AssertCalled
            override fun onAuthPrompt(session: GeckoSession, prompt: AuthPrompt): GeckoResult<PromptResponse>? {
                return GeckoResult.fromValue(prompt.confirm("foo", "bar"));
            }

            @AssertCalled
            override fun onLoginFetch(domain: String): GeckoResult<Array<Autocomplete.LoginEntry>>? {
                return GeckoResult.fromValue(arrayOf());
            }

            @AssertCalled
            override fun onLoginSave(
                session: GeckoSession,
                request: PromptDelegate.AutocompleteRequest<Autocomplete.LoginSaveOption>
            ): GeckoResult<PromptResponse>? {
                val authInfo = request.options[0].value
                assertThat("auth matches", authInfo.formActionOrigin, isEmptyOrNullString())
                assertThat("auth matches", authInfo.httpRealm, equalTo("Fake Realm"))
                assertThat("auth matches", authInfo.origin, equalTo(GeckoSessionTestRule.TEST_ENDPOINT))
                assertThat("auth matches", authInfo.username, equalTo("foo"))
                assertThat("auth matches", authInfo.password, equalTo("bar"))
                promptInstanceDelegate.prompt = request
                request.setDelegate(promptInstanceDelegate)
                return GeckoResult()
            }
        })

        mainSession.loadTestPath("/basic-auth/foo/bar")

        // The server we try to hit will always reject the login so we should
        // get a request to reauth which should dismiss the prompt
        val actualPrompt = sessionRule.waitForResult(result)

        assertThat("Prompt object should match", actualPrompt, equalTo(promptInstanceDelegate.prompt))
    }

    @Test fun dismissAuthTest() {
        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 2)
            override fun onAuthPrompt(session: GeckoSession, prompt: PromptDelegate.AuthPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                //TODO: Figure out some better testing here.
                return null
            }
        })

        mainSession.loadTestPath("/basic-auth/foo/bar")
        mainSession.waitForPageStop()

        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test fun buttonTest() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, prompt: PromptDelegate.ButtonPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Confirm?", equalTo(prompt.message))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.ButtonPrompt.Type.POSITIVE))
            }
        })

        assertThat("Result should match",
                sessionRule.session.waitForJS("confirm('Confirm?')") as Boolean,
                equalTo(true))

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, prompt: PromptDelegate.ButtonPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Confirm?", equalTo(prompt.message))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.ButtonPrompt.Type.NEGATIVE))
            }
        })

        assertThat("Result should match",
                sessionRule.session.waitForJS("confirm('Confirm?')") as Boolean,
                equalTo(false))
    }

    @Test
    fun onFormResubmissionPrompt() {
        sessionRule.session.loadTestPath(RESUBMIT_CONFIRM)
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS(
            "document.querySelector('#text').value = 'Some text';" +
            "document.querySelector('#submit').click();"
        )

        // Submitting the form causes a navigation
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object: ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("Only HELLO_HTML_PATH should load", url, endsWith(HELLO_HTML_PATH))
                result.complete(null)
            }
        })

        val promptResult = GeckoResult<PromptDelegate.PromptResponse>()
        val promptResult2 = GeckoResult<PromptDelegate.PromptResponse>()

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 2)
            override fun onRepostConfirmPrompt(session: GeckoSession, prompt: PromptDelegate.RepostConfirmPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                // We have to return something here because otherwise the delegate will be invoked
                // before we have a chance to override it in the waitUntilCalled call below
                return forEachCall(promptResult, promptResult2)
            }
        })

        // This should trigger a confirm resubmit prompt
        sessionRule.session.reload();

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onRepostConfirmPrompt(session: GeckoSession, prompt: PromptDelegate.RepostConfirmPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult.complete(prompt.confirm(AllowOrDeny.DENY))
                return promptResult
            }
        })

        sessionRule.waitForResult(promptResult)

        // Trigger it again, this time the load should go through
        sessionRule.session.reload();
        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onRepostConfirmPrompt(session: GeckoSession, prompt: PromptDelegate.RepostConfirmPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult2.complete(prompt.confirm(AllowOrDeny.ALLOW))
                return promptResult2
            }
        })

        sessionRule.waitForResult(promptResult2)
        sessionRule.waitForResult(result)
    }

    @Test
    fun onBeforeUnloadTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "dom.require_user_interaction_for_beforeunload" to false
        ))
        sessionRule.session.loadTestPath(BEFORE_UNLOAD)
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object: ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("Only HELLO2_HTML_PATH should load", url, endsWith(HELLO2_HTML_PATH))
                result.complete(null)
            }
        })

        val promptResult = GeckoResult<PromptDelegate.PromptResponse>()
        val promptResult2 = GeckoResult<PromptDelegate.PromptResponse>()

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 2)
            override fun onBeforeUnloadPrompt(session: GeckoSession, prompt: PromptDelegate.BeforeUnloadPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                // We have to return something here because otherwise the delegate will be invoked
                // before we have a chance to override it in the waitUntilCalled call below
                return forEachCall(promptResult, promptResult2)
            }
        })

        // This will try to load "hello.html" but will be denied, if the request
        // goes through anyway the onLoadRequest delegate above will throw an exception
        sessionRule.session.evaluateJS("document.querySelector('#navigateAway').click()")
        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onBeforeUnloadPrompt(session: GeckoSession, prompt: PromptDelegate.BeforeUnloadPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult.complete(prompt.confirm(AllowOrDeny.DENY))
                return promptResult
            }
        })

        sessionRule.waitForResult(promptResult)

        // This request will go through and end the test. Doing the negative case first will
        // ensure that if either of this tests fail the test will fail.
        sessionRule.session.evaluateJS("document.querySelector('#navigateAway2').click()")
        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onBeforeUnloadPrompt(session: GeckoSession, prompt: PromptDelegate.BeforeUnloadPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult2.complete(prompt.confirm(AllowOrDeny.ALLOW))
                return promptResult2
            }
        })

        sessionRule.waitForResult(promptResult2)
        sessionRule.waitForResult(result)
    }

    @Test fun textTest() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onTextPrompt(session: GeckoSession, prompt: PromptDelegate.TextPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Prompt:", equalTo(prompt.message))
                assertThat("Default should match", "default", equalTo(prompt.defaultValue))
                return GeckoResult.fromValue(prompt.confirm("foo"))
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

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @Test fun colorTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onColorPrompt(session: GeckoSession, prompt: PromptDelegate.ColorPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Value should match", "#ffffff", equalTo(prompt.defaultValue))
                return GeckoResult.fromValue(prompt.confirm("#123456"))
            }
        })

        sessionRule.session.evaluateJS("""
            this.c = document.getElementById('colorexample');
        """.trimIndent())

        val promise = sessionRule.session.evaluatePromiseJS("""
            new Promise((resolve, reject) => {
                this.c.addEventListener(
                    'change',
                    event => resolve(event.target.value),
                    false
                );
            })""".trimIndent())

        sessionRule.session.evaluateJS("this.c.click();")

        assertThat("Value should match",
                promise.value as String,
                equalTo("#123456"))
    }

    @Ignore // TODO: Figure out weird test env behavior here.
    @Test fun dateTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.getElementById('dateexample').click();")

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @Test fun fileTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        sessionRule.session.loadTestPath(PROMPT_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("document.getElementById('fileexample').click();")

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onFilePrompt(session: GeckoSession, prompt: PromptDelegate.FilePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Length of mimeTypes should match", 2, equalTo(prompt.mimeTypes!!.size))
                assertThat("First accept attribute should match", "image/*", equalTo(prompt.mimeTypes?.get(0)))
                assertThat("Second accept attribute should match", ".pdf", equalTo(prompt.mimeTypes?.get(1)))
                assertThat("Capture attribute should match", PromptDelegate.FilePrompt.Capture.USER, equalTo(prompt.capture))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @Test fun shareTextSucceeds() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareText = "Example share text"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Text field is not null", prompt.text, notNullValue())
                assertThat("Title field is null", prompt.title, nullValue())
                assertThat("Url field is null", prompt.uri, nullValue())
                assertThat("Text field contains correct value", prompt.text, equalTo(shareText))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.SharePrompt.Result.SUCCESS))
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({text: "${shareText}"})""")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            Assert.fail("Share must succeed." + e.reason as String)
        }
    }

    @Test fun shareUrlSucceeds() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareUrl = "https://example.com/"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Text field is null", prompt.text, nullValue())
                assertThat("Title field is null", prompt.title, nullValue())
                assertThat("Url field is not null", prompt.uri, notNullValue())
                assertThat("Text field contains correct value", prompt.uri, equalTo(shareUrl))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.SharePrompt.Result.SUCCESS))
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            Assert.fail("Share must succeed." + e.reason as String)
        }
    }

    @Test fun shareTitleSucceeds() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareTitle = "Title!"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Text field is null", prompt.text, nullValue())
                assertThat("Title field is not null", prompt.title, notNullValue())
                assertThat("Url field is null", prompt.uri, nullValue())
                assertThat("Text field contains correct value", prompt.title, equalTo(shareTitle))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.SharePrompt.Result.SUCCESS))
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({title: "${shareTitle}"})""")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            Assert.fail("Share must succeed." + e.reason as String)
        }
    }

    @Test fun failedShareReturnsDataError() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareUrl = "https://www.example.com"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.SharePrompt.Result.FAILURE))
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("DataError"))
        }
    }

    @Test fun abortedShareReturnsAbortError() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareUrl = "https://www.example.com"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.SharePrompt.Result.ABORT))
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("AbortError"))
        }
    }

    @Test fun dismissedShareReturnsAbortError() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareUrl = "https://www.example.com"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("AbortError"))
        }
    }

    @Test fun emptyShareReturnsTypeError() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 0)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("TypeError"))
        }
    }

    @Test fun invalidShareUrlReturnsTypeError() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        // Invalid port should cause URL parser to fail.
        val shareUrl = "http://www.example.com:123456"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 0)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("TypeError"))
        }
    }

    @Test fun shareRequiresUserInteraction() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webshare.requireinteraction" to true))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val shareUrl = "https://www.example.com"

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 0)
            override fun onSharePrompt(session: GeckoSession, prompt: PromptDelegate.SharePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        try {
            mainSession.waitForJS("""window.navigator.share({url: "${shareUrl}"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("NotAllowedError"))
        }
    }
}
