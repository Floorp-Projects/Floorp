/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.view.KeyEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assert
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.Autocomplete
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
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.TestServer
import java.util.UUID

@RunWith(AndroidJUnit4::class)
@MediumTest
class PromptDelegateTest : BaseSessionTest(
    serverCustomHeaders = mapOf(
        "Access-Control-Allow-Origin" to "*",
    ),
    responseModifiers = mapOf(
        "/assets/www/fedcm_accounts_endpoint.json" to TestServer.ResponseModifier { response ->
            response.replace("\$RANDOM_ID", UUID.randomUUID().toString())
        },
    ),
) {
    @Test fun popupTestAllow() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateDuringNextWait(object : PromptDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupPrompt(session: GeckoSession, prompt: PromptDelegate.PopupPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", prompt.targetUri, notNullValue())
                assertThat("URL should match", prompt.targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(prompt.confirm(AllowOrDeny.ALLOW))
            }

            @AssertCalled(count = 2)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ): GeckoResult<AllowOrDeny>? {
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

        mainSession.loadTestPath(POPUP_HTML_PATH)
        sessionRule.waitUntilCalled(NavigationDelegate::class, "onNewSession")
    }

    @Test fun popupTestBlock() {
        // Ensure popup blocking is enabled for this test.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to true))

        sessionRule.delegateUntilTestEnd(object : PromptDelegate, NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onPopupPrompt(session: GeckoSession, prompt: PromptDelegate.PopupPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", prompt.targetUri, notNullValue())
                assertThat("URL should match", prompt.targetUri, endsWith(HELLO_HTML_PATH))
                return GeckoResult.fromValue(prompt.confirm(AllowOrDeny.DENY))
            }

            @AssertCalled(count = 1)
            override fun onLoadRequest(
                session: GeckoSession,
                request: LoadRequest,
            ): GeckoResult<AllowOrDeny>? {
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

        mainSession.loadTestPath(POPUP_HTML_PATH)
        sessionRule.waitForPageStop()
        mainSession.waitForRoundTrip()
    }

    @Ignore // TODO: Reenable when 1501574 is fixed.
    @Test
    fun alertTest() {
        mainSession.evaluateJS("alert('Alert!');")

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
                return GeckoResult.fromValue(
                    arrayOf(
                        Autocomplete.LoginEntry.Builder()
                            .origin(GeckoSessionTestRule.TEST_ENDPOINT)
                            .formActionOrigin(GeckoSessionTestRule.TEST_ENDPOINT)
                            .httpRealm("Fake Realm")
                            .username("test-username")
                            .password("test-password")
                            .formActionOrigin(null)
                            .guid("test-guid")
                            .build(),
                    ),
                )
            }
        })
        sessionRule.waitUntilCalled(object : PromptDelegate, Autocomplete.StorageDelegate {
            @AssertCalled
            override fun onAuthPrompt(session: GeckoSession, prompt: AuthPrompt): GeckoResult<PromptResponse>? {
                assertThat(
                    "Saved login should appear here",
                    prompt.authOptions.username,
                    equalTo("test-username"),
                )
                assertThat(
                    "Saved login should appear here",
                    prompt.authOptions.password,
                    equalTo("test-password"),
                )
                return null
            }
        })
    }

    // This test checks that we store login information submitted through HTTP basic auth
    // This also tests that the login save prompt gets automatically dismissed if
    // the login information is incorrect.
    @Test fun loginStorageHttpAuth() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "signon.rememberSignons" to true,
            ),
        )
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
                return GeckoResult.fromValue(prompt.confirm("foo", "bar"))
            }

            @AssertCalled
            override fun onLoginFetch(domain: String): GeckoResult<Array<Autocomplete.LoginEntry>>? {
                return GeckoResult.fromValue(arrayOf())
            }

            @AssertCalled
            override fun onLoginSave(
                session: GeckoSession,
                request: PromptDelegate.AutocompleteRequest<Autocomplete.LoginSaveOption>,
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
                // TODO: Figure out some better testing here.
                return null
            }
        })

        mainSession.loadTestPath("/basic-auth/foo/bar")
        mainSession.waitForPageStop()

        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test fun buttonTest() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, prompt: PromptDelegate.ButtonPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Confirm?", equalTo(prompt.message))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.ButtonPrompt.Type.POSITIVE))
            }
        })

        assertThat(
            "Result should match",
            mainSession.waitForJS("confirm('Confirm?')") as Boolean,
            equalTo(true),
        )

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onButtonPrompt(session: GeckoSession, prompt: PromptDelegate.ButtonPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Confirm?", equalTo(prompt.message))
                return GeckoResult.fromValue(prompt.confirm(PromptDelegate.ButtonPrompt.Type.NEGATIVE))
            }
        })

        assertThat(
            "Result should match",
            mainSession.waitForJS("confirm('Confirm?')") as Boolean,
            equalTo(false),
        )
    }

    @Test
    fun onFormResubmissionPrompt() {
        mainSession.loadTestPath(RESUBMIT_CONFIRM)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS(
            "document.querySelector('#text').value = 'Some text';" +
                "document.querySelector('#submit').click();",
        )

        // Submitting the form causes a navigation
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
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
        mainSession.reload()

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onRepostConfirmPrompt(session: GeckoSession, prompt: PromptDelegate.RepostConfirmPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult.complete(prompt.confirm(AllowOrDeny.DENY))
                return promptResult
            }
        })

        sessionRule.waitForResult(promptResult)

        // Trigger it again, this time the load should go through
        mainSession.reload()
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
    @WithDisplay(width = 100, height = 100)
    fun selectTestSimple() {
        mainSession.loadTestPath(SELECT_HTML_PATH)
        sessionRule.waitForPageStop()

        val result = GeckoResult<PromptDelegate.PromptResponse>()
        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should not be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.SINGLE))
                assertThat("There should be two choices", prompt.choices.size, equalTo(2))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                result.complete(prompt.confirm(prompt.choices[1]))
                return result
            }
        })

        val promise = mainSession.evaluatePromiseJS(
            """new Promise(function(resolve) {
            let events = [];
            // Record the events for testing purposes.
            for (const t of ["change", "input"]) {
                document.querySelector("select").addEventListener(t, function(e) {
                    events.push(e.type + "(composed=" + e.composed + ")");
                    if (events.length == 2) {
                        resolve(events.join(" "));
                    }
                });
            }
        })""",
        )

        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
        assertThat(
            "Events should be as expected",
            promise.value as String,
            equalTo("input(composed=true) change(composed=false)"),
        )
    }

    @Test
    @WithDisplay(width = 100, height = 100)
    fun selectTestSize() {
        mainSession.loadTestPath(SELECT_LISTBOX_HTML_PATH)
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should not be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.SINGLE))
                assertThat("There should be three choices", prompt.choices.size, equalTo(3))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                assertThat("Third choice is correct", prompt.choices[2].label, equalTo("GHI"))
                result.complete(null)
                return null
            }
        })

        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
    }

    @Test
    @WithDisplay(width = 100, height = 100)
    fun selectTestMultiple() {
        mainSession.loadTestPath(SELECT_MULTIPLE_HTML_PATH)
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.MULTIPLE))
                assertThat("There should be three choices", prompt.choices.size, equalTo(3))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                assertThat("Third choice is correct", prompt.choices[2].label, equalTo("GHI"))
                result.complete(null)
                return null
            }
        })

        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
    }

    @Test
    @WithDisplay(width = 100, height = 100)
    fun selectTestShowPicker() {
        mainSession.loadTestPath(SELECT_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('simple').showPicker()
            });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should not be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.SINGLE))
                assertThat("There should be two choices", prompt.choices.size, equalTo(2))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                return null
            }
        })

        mainSession.loadTestPath(SELECT_MULTIPLE_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('multiple').showPicker()
            });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.MULTIPLE))
                assertThat("There should be three choices", prompt.choices.size, equalTo(3))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                assertThat("Third choice is correct", prompt.choices[2].label, equalTo("GHI"))
                return null
            }
        })

        mainSession.loadTestPath(SELECT_LISTBOX_HTML_PATH)
        sessionRule.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('multiple').showPicker()
            });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Should not be multiple", prompt.type, equalTo(PromptDelegate.ChoicePrompt.Type.SINGLE))
                assertThat("There should be three choices", prompt.choices.size, equalTo(3))
                assertThat("First choice is correct", prompt.choices[0].label, equalTo("ABC"))
                assertThat("Second choice is correct", prompt.choices[1].label, equalTo("DEF"))
                assertThat("Third choice is correct", prompt.choices[2].label, equalTo("GHI"))
                return null
            }
        })
    }

    @Test
    @WithDisplay(width = 100, height = 100)
    fun selectTestUpdate() {
        mainSession.loadTestPath(SELECT_HTML_PATH)
        sessionRule.waitForPageStop()

        val result = GeckoResult<PromptDelegate.PromptResponse>()
        val promptInstanceDelegate = object : PromptDelegate.PromptInstanceDelegate {
            override fun onPromptUpdate(prompt: PromptDelegate.BasePrompt) {
                val newPrompt: PromptDelegate.ChoicePrompt = prompt as PromptDelegate.ChoicePrompt
                assertThat("First choice is correct", newPrompt.choices[0].label, equalTo("foo"))
                assertThat("Second choice is correct", newPrompt.choices[1].label, equalTo("bar"))
                assertThat("Third choice is correct", newPrompt.choices[2].label, equalTo("baz"))
                result.complete(prompt.confirm(newPrompt.choices[2]))
            }
        }

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("There should be two choices", prompt.choices.size, equalTo(2))
                prompt.setDelegate(promptInstanceDelegate)
                return result
            }
        })

        mainSession.evaluateJS(
            """
            document.querySelector("select").addEventListener("focus", () => {
                window.setTimeout(() => {
                    document.querySelector("select").innerHTML =
                        "<option>foo</option><option>bar</option><option>baz</option>";
                }, 100);
            }, { once: true })
            """.trimIndent(),
        )

        val promise = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                document.querySelector("select").addEventListener("change", e => {
                    resolve(e.target.value);
                });
            })
            """.trimIndent(),
        )

        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
        assertThat(
            "Selected item should be as expected",
            promise.value as String,
            equalTo("baz"),
        )
    }

    @Test
    @WithDisplay(width = 100, height = 100)
    fun selectTestDismiss() {
        mainSession.loadTestPath(SELECT_HTML_PATH)
        sessionRule.waitForPageStop()

        val result = GeckoResult<PromptDelegate.PromptResponse>()
        val promptInstanceDelegate = object : PromptDelegate.PromptInstanceDelegate {
            override fun onPromptDismiss(prompt: PromptDelegate.BasePrompt) {
                result.complete(prompt.dismiss())
            }
        }

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onChoicePrompt(session: GeckoSession, prompt: PromptDelegate.ChoicePrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("There should be two choices", prompt.choices.size, equalTo(2))
                prompt.setDelegate(promptInstanceDelegate)
                mainSession.evaluateJS("document.querySelector('select').blur()")
                return result
            }
        })

        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
    }

    @Test
    fun onBeforeUnloadTest() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.require_user_interaction_for_beforeunload" to false,
            ),
        )
        mainSession.loadTestPath(BEFORE_UNLOAD)
        sessionRule.waitForPageStop()

        val result = GeckoResult<Void>()
        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
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
        mainSession.evaluateJS("document.querySelector('#navigateAway').click()")
        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onBeforeUnloadPrompt(session: GeckoSession, prompt: PromptDelegate.BeforeUnloadPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                promptResult.complete(prompt.confirm(AllowOrDeny.DENY))
                return promptResult
            }
        })

        sessionRule.waitForResult(promptResult)

        // Although onBeforeUnloadPrompt is done, nsDocumentViewer might not clear
        // mInPermitUnloadPrompt flag at this time yet. We need a wait to finish
        // "nsDocumentViewer::PermitUnload" loop.
        mainSession.waitForJS("new Promise(resolve => window.setTimeout(resolve, 100))")

        // This request will go through and end the test. Doing the negative case first will
        // ensure that if either of this tests fail the test will fail.
        mainSession.evaluateJS("document.querySelector('#navigateAway2').click()")
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
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onTextPrompt(session: GeckoSession, prompt: PromptDelegate.TextPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Message should match", "Prompt:", equalTo(prompt.message))
                assertThat("Default should match", "default", equalTo(prompt.defaultValue))
                return GeckoResult.fromValue(prompt.confirm("foo"))
            }
        })

        assertThat(
            "Result should match",
            mainSession.waitForJS("prompt('Prompt:', 'default')") as String,
            equalTo("foo"),
        )
    }

    @Test
    fun fedCMProviderPromptTest() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.security.credentialmanagement.identity.enabled" to true,
            ),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.security.credentialmanagement.identity.heavyweight.enabled" to true,
            ),
        )
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.security.credentialmanagement.identity.test_ignore_well_known" to true,
            ),
        )
        mainSession.loadTestPath(FEDCM_RP_HTML_PATH)

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onSelectIdentityCredentialProvider(
                session: GeckoSession,
                prompt: PromptDelegate.IdentityCredential.ProviderSelectorPrompt,
            ): GeckoResult<PromptResponse> {
                prompt.providers.mapIndexed { index, item ->
                    assertThat("ID should match", index, equalTo(item.id))
                    assertThat(
                        "Name should be the name of the IDP taken from the manifest",
                        item.name,
                        containsString("Demo IDP"),
                    )
                    assertThat("Icon should contain a valid image", item.icon ?: "", containsString("data:image"))
                }
                return GeckoResult.fromValue(prompt.confirm(0))
            }

            @AssertCalled(count = 1)
            override fun onSelectIdentityCredentialAccount(
                session: GeckoSession,
                prompt: PromptDelegate.IdentityCredential.AccountSelectorPrompt,
            ): GeckoResult<PromptResponse> {
                prompt.accounts.forEachIndexed { index, item ->
                    assertThat("ID should match", index, equalTo(item.id))
                }
                return GeckoResult.fromValue(prompt.confirm(0))
            }

            @AssertCalled(count = 1)
            override fun onShowPrivacyPolicyIdentityCredential(
                session: GeckoSession,
                prompt: PromptDelegate.IdentityCredential.PrivacyPolicyPrompt,
            ): GeckoResult<PromptResponse> {
                assertThat("Host should be localhost", prompt.host, equalTo("localhost"))
                assertThat("Privacy policy url should be the same as specified in fedcm_idp_metadata.json ", prompt.privacyPolicyUrl, equalTo("privacy_policy"))
                assertThat("Terms of service url should be the same as specified in fedcm_idp_metadata.json ", prompt.termsOfServiceUrl, equalTo("terms_of_service"))
                assertThat("Icon should contain a valid image", prompt.icon ?: "", containsString("data:image"))
                return GeckoResult.fromValue(prompt.confirm(true))
            }
        })

        mainSession.waitForJS(
            """
        navigator.credentials.get({
        identity: {
          providers: [{
            configURL: "${createTestUrl(FEDCM_IDP_MANIFEST_PATH)}",
            clientId: "localhost",
            nonce: "nonce",
          }]
        }
      });
            """.trimIndent(),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun colorTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onColorPrompt(session: GeckoSession, prompt: PromptDelegate.ColorPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Value should match", "#ffffff", equalTo(prompt.defaultValue))
                assertThat("Predefined values size", 0, equalTo(prompt.predefinedValues!!.size))
                return GeckoResult.fromValue(prompt.confirm("#123456"))
            }
        })

        mainSession.evaluateJS(
            """
            this.c = document.getElementById('colorexample');
            """.trimIndent(),
        )

        val promise = mainSession.evaluatePromiseJS(
            """
            new Promise((resolve, reject) => {
                this.c.addEventListener(
                    'change',
                    event => resolve(event.target.value),
                    false
                );
            })
            """.trimIndent(),
        )

        mainSession.evaluateJS("document.addEventListener('click', () => this.c.click(), { once: true });")
        mainSession.synthesizeTap(1, 1)

        assertThat(
            "Value should match",
            promise.value as String,
            equalTo("#123456"),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun colorTestWithDatalist() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onColorPrompt(session: GeckoSession, prompt: PromptDelegate.ColorPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("Value should match", "#ffffff", equalTo(prompt.defaultValue))
                assertThat("Predefined values size", 2, equalTo(prompt.predefinedValues!!.size))
                assertThat("First predefined value", "#000000", equalTo(prompt.predefinedValues?.get(0)))
                assertThat("Second predefined value", "#808080", equalTo(prompt.predefinedValues?.get(1)))
                return GeckoResult.fromValue(prompt.confirm("#123456"))
            }
        })

        mainSession.evaluateJS(
            """
            this.c = document.getElementById('colorexample');
            this.c.setAttribute('list', 'colorlist');
            """.trimIndent(),
        )

        val promise = mainSession.evaluatePromiseJS(
            """
            new Promise((resolve, reject) => {
                this.c.addEventListener(
                    'change',
                    event => resolve(event.target.value),
                );
            })
            """.trimIndent(),
        )

        mainSession.evaluateJS("document.addEventListener('click', () => this.c.click(), { once: true });")
        mainSession.synthesizeTap(1, 1)

        assertThat(
            "Value should match",
            promise.value as String,
            equalTo("#123456"),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dateTest() {
        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.body.addEventListener("click", () => {
                document.getElementById('dateexample').showPicker();
            });
            """.trimIndent(),
        )

        mainSession.synthesizeTap(1, 1) // Provides user activation.
        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dateTestByTap() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        // By removing first element in PROMPT_HTML_PATH, dateexample becomes first element.
        //
        // TODO: What better calculation of element bounds for synthesizeTap?
        mainSession.evaluateJS(
            """
            document.getElementById('selectexample').remove();
            document.getElementById('dateexample').getBoundingClientRect();
            """.trimIndent(),
        )
        mainSession.synthesizeTap(10, 10)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("<input type=date> is tapped", PromptDelegate.DateTimePrompt.Type.DATE, equalTo(prompt.type))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun monthTestByTap() {
        // Gecko doesn't have the widget for <input type=month>. But GeckoView can show the picker.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        // TODO: What better calculation of element bounds for synthesizeTap?
        mainSession.evaluateJS(
            """
            document.getElementById('selectexample').remove();
            document.getElementById('dateexample').remove();
            document.getElementById('weekexample').getBoundingClientRect();
            """.trimIndent(),
        )
        mainSession.synthesizeTap(10, 10)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("<input type=month> is tapped", PromptDelegate.DateTimePrompt.Type.MONTH, equalTo(prompt.type))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dateTestParameters() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS(
            """
            document.getElementById('selectexample').remove();
            document.getElementById('dateexample').min = "2022-01-01";
            document.getElementById('dateexample').max = "2022-12-31";
            document.getElementById('dateexample').step = "10";
            document.getElementById('dateexample').getBoundingClientRect();
            """.trimIndent(),
        )
        mainSession.synthesizeTap(10, 10)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("<input type=date> is tapped", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.DATE))
                assertThat("min value is exported", prompt.minValue, equalTo("2022-01-01"))
                assertThat("max value is exported", prompt.maxValue, equalTo("2022-12-31"))
                assertThat("step value is exported", prompt.stepValue, equalTo("10"))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dateTestDismiss() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        val result = GeckoResult<PromptDelegate.PromptResponse>()
        val promptInstanceDelegate = object : PromptDelegate.PromptInstanceDelegate {
            override fun onPromptDismiss(prompt: PromptDelegate.BasePrompt) {
                result.complete(prompt.dismiss())
            }
        }

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("<input type=date> is tapped", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.DATE))
                prompt.setDelegate(promptInstanceDelegate)
                mainSession.evaluateJS("document.getElementById('dateexample').blur()")
                return result
            }
        })

        mainSession.evaluateJS("document.getElementById('selectexample').remove()")
        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun monthTestDismiss() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        val result = GeckoResult<PromptDelegate.PromptResponse>()
        val promptInstanceDelegate = object : PromptDelegate.PromptInstanceDelegate {
            override fun onPromptDismiss(prompt: PromptDelegate.BasePrompt) {
                result.complete(prompt.dismiss())
            }
        }

        sessionRule.delegateUntilTestEnd(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("<input type=month> is tapped", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.MONTH))
                prompt.setDelegate(promptInstanceDelegate)
                mainSession.evaluateJS("document.getElementById('monthexample').blur()")
                return result
            }
        })

        mainSession.evaluateJS(
            """
            document.getElementById('selectexample').remove();
            document.getElementById('dateexample').remove();
            """.trimIndent(),
        )
        mainSession.synthesizeTap(10, 10)
        sessionRule.waitForResult(result)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dateMonthTestShowPicker() {
        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        // type=month and type=week have no custom controls on all platforms.
        // But mobile has the picker with dom.forms.datetime.others=true

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('monthexample').showPicker()
            }, { once: true });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("showPicker for <input type=month>", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.MONTH))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('weekexample').showPicker()
            }, { once: true });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("showPicker for <input type=week>", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.WEEK))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        // desktop has no type=time picker, but mobile has.

        mainSession.evaluateJS(
            """
            document.body.focus();
            document.body.addEventListener('keydown', () => {
                document.getElementById('timeexample').showPicker()
            }, { once: true });
            """.trimIndent(),
        )
        mainSession.pressKey(KeyEvent.KEYCODE_SPACE)

        sessionRule.waitUntilCalled(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onDateTimePrompt(session: GeckoSession, prompt: PromptDelegate.DateTimePrompt): GeckoResult<PromptDelegate.PromptResponse> {
                assertThat("showPicker for <input type=time>", prompt.type, equalTo(PromptDelegate.DateTimePrompt.Type.TIME))
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun fileTest() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.disable_open_during_load" to false))

        mainSession.loadTestPath(PROMPT_HTML_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("document.addEventListener('click', () => document.getElementById('fileexample').click(), { once: true });")
        mainSession.synthesizeTap(1, 1)


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
            mainSession.waitForJS("""window.navigator.share({text: "$shareText"})""")
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
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
            mainSession.waitForJS("""window.navigator.share({title: "$shareTitle"})""")
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("DataError"),
            )
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("AbortError"),
            )
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("AbortError"),
            )
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
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("TypeError"),
            )
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("TypeError"),
            )
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
            mainSession.waitForJS("""window.navigator.share({url: "$shareUrl"})""")
            Assert.fail("Request should have failed")
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            assertThat(
                "Error should be correct",
                e.reason as String,
                containsString("NotAllowedError"),
            )
        }
    }
}
