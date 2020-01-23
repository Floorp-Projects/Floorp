/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.hamcrest.Matchers.*

import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate.LoginStoragePrompt
import org.mozilla.geckoview.LoginStorage
import org.mozilla.geckoview.LoginStorage.LoginEntry
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks


@RunWith(AndroidJUnit4::class)
@MediumTest
class LoginStorageDelegateTest : BaseSessionTest() {

    @Test
    fun fetchLogins() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        val fetchHandled = GeckoResult<Void>()

        sessionRule.addExternalDelegateDuringNextWait(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled(count = 1)
            override fun onLoginFetch(domain: String)
                    : GeckoResult<Array<LoginEntry>>? {
                assertThat("Domain should match", domain, equalTo("localhost"))

                fetchHandled.complete(null)
                return null
            }
        })

        mainSession.loadTestPath(FORMS3_HTML_PATH)
        sessionRule.waitForResult(fetchHandled)
    }

    @Test
    fun loginSaveDismiss() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        sessionRule.addExternalDelegateDuringNextWait(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled(count = 1)
            override fun onLoginFetch(domain: String)
                    : GeckoResult<Array<LoginEntry>>? {
                assertThat("Domain should match", domain, equalTo("localhost"))

                return null
            }
        })

        mainSession.loadTestPath(FORMS3_HTML_PATH)
        mainSession.waitForPageStop()

        sessionRule.addExternalDelegateUntilTestEnd(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled(count = 0)
            override fun onLoginSave(login: LoginEntry) {}
        })

        // Assign login credentials.
        mainSession.evaluateJS("document.querySelector('#user1').value = 'user1x'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = 'pass1x'")

        // Submit the form.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitUntilCalled(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onLoginStoragePrompt(
                    session: GeckoSession,
                    prompt: LoginStoragePrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                val login = prompt.logins[0]

                assertThat("Session should not be null", session, notNullValue())
                assertThat(
                    "Type should match",
                    prompt.type,
                    equalTo(LoginStoragePrompt.Type.SAVE))
                assertThat("Login should not be null", login, notNullValue())
                assertThat(
                    "Username should match",
                    login.username,
                    equalTo("user1x"))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo("pass1x"))

                return GeckoResult.fromValue(prompt.dismiss())
            }
        })
    }

    @Test
    fun loginSaveAccept() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        mainSession.loadTestPath(FORMS3_HTML_PATH)
        mainSession.waitForPageStop()

        val saveHandled = GeckoResult<Void>()

        sessionRule.addExternalDelegateUntilTestEnd(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled
            override fun onLoginSave(login: LoginEntry) {
                assertThat(
                    "Username should match",
                    login.username,
                    equalTo("user1x"))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo("pass1x"))

                saveHandled.complete(null)
            }
        })

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onLoginStoragePrompt(
                    session: GeckoSession,
                    prompt: LoginStoragePrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat(
                    "Type should match",
                    prompt.type,
                    equalTo(LoginStoragePrompt.Type.SAVE))

                val login = prompt.logins[0]

                assertThat("Login should not be null", login, notNullValue())

                assertThat(
                    "Username should match",
                    login.username,
                    equalTo("user1x"))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo("pass1x"))

                return GeckoResult.fromValue(prompt.confirm(login))
            }
        })

        // Assign login credentials.
        mainSession.evaluateJS("document.querySelector('#user1').value = 'user1x'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = 'pass1x'")

        // Submit the form.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitForResult(saveHandled)
    }

    @Test
    fun loginSaveModifyAccept() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        mainSession.loadTestPath(FORMS3_HTML_PATH)
        mainSession.waitForPageStop()

        val saveHandled = GeckoResult<Void>()

        sessionRule.addExternalDelegateUntilTestEnd(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled
            override fun onLoginSave(login: LoginEntry) {
                assertThat(
                    "Username should match",
                    login.username,
                    equalTo("user1x"))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo("pass1xmod"))

                saveHandled.complete(null)
            }
        })

        sessionRule.delegateDuringNextWait(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onLoginStoragePrompt(
                    session: GeckoSession,
                    prompt: LoginStoragePrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat(
                    "Type should match",
                    prompt.type,
                    equalTo(LoginStoragePrompt.Type.SAVE))

                val login = prompt.logins[0]

                assertThat("Login should not be null", login, notNullValue())

                assertThat(
                    "Username should match",
                    login.username,
                    equalTo("user1x"))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo("pass1x"))

                val modLogin = LoginEntry.Builder()
                        .origin(login.origin)
                        .formActionOrigin(login.origin)
                        .httpRealm(login.httpRealm)
                        .username(login.username)
                        .password("pass1xmod")
                        .build()

                return GeckoResult.fromValue(prompt.confirm(modLogin))
            }
        })

        // Assign login credentials.
        mainSession.evaluateJS("document.querySelector('#user1').value = 'user1x'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = 'pass1x'")

        // Submit the form.
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitForResult(saveHandled)
    }

    @Test
    fun loginUpdateAccept() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        val saveHandled = GeckoResult<Void>()
        val saveHandled2 = GeckoResult<Void>()

        val user1 = "user1x"
        val pass1 = "pass1x"
        val pass2 = "pass1up"
        val guid = "test-guid"
        val savedLogins = mutableListOf<LoginEntry>()

        sessionRule.addExternalDelegateUntilTestEnd(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled
            override fun onLoginFetch(domain: String)
                    : GeckoResult<Array<LoginEntry>>? {
                assertThat("Domain should match", domain, equalTo("localhost"))

                return GeckoResult.fromValue(savedLogins.toTypedArray())
            }

            @AssertCalled(count = 2)
            override fun onLoginSave(login: LoginEntry) {
                assertThat(
                    "Username should match",
                    login.username,
                    equalTo(user1))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo(forEachCall(pass1, pass2)))

                assertThat(
                    "GUID should match",
                    login.guid,
                    equalTo(forEachCall(null, guid)))

                val savedLogin = LoginEntry.Builder()
                        .guid(guid)
                        .origin(login.origin)
                        .formActionOrigin(login.formActionOrigin)
                        .username(login.username)
                        .password(login.password)
                        .build()

                savedLogins.add(savedLogin)

                if (sessionRule.currentCall.counter == 1) {
                    saveHandled.complete(null)
                } else if (sessionRule.currentCall.counter == 2) {
                    saveHandled2.complete(null)
                }
            }
        })

        sessionRule.delegateUntilTestEnd(object : Callbacks.PromptDelegate {
            @AssertCalled(count = 2)
            override fun onLoginStoragePrompt(
                    session: GeckoSession,
                    prompt: LoginStoragePrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                assertThat("Session should not be null", session, notNullValue())
                assertThat(
                    "Type should match",
                    prompt.type,
                    equalTo(LoginStoragePrompt.Type.SAVE))

                val login = prompt.logins[0]

                assertThat("Login should not be null", login, notNullValue())

                assertThat(
                    "Username should match",
                    login.username,
                    equalTo(user1))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo(forEachCall(pass1, pass2)))

                return GeckoResult.fromValue(prompt.confirm(login))
            }
        })

        // Assign login credentials.
        mainSession.loadTestPath(FORMS3_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.evaluateJS("document.querySelector('#user1').value = '$user1'")
        mainSession.evaluateJS("document.querySelector('#pass1').value = '$pass1'")
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitForResult(saveHandled)

        // Update login credentials.
        val session2 = sessionRule.createOpenSession()
        session2.loadTestPath(FORMS3_HTML_PATH)
        session2.waitForPageStop()
        session2.evaluateJS("document.querySelector('#pass1').value = '$pass2'")
        session2.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitForResult(saveHandled2)
    }

    @Test
    fun loginUsed() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                // Enable login management since it's disabled in automation.
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true,
                "signon.userInputRequiredToCapture.enabled" to false))

        val runtime = sessionRule.runtime
        val register = { delegate: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = delegate
        }
        val unregister = { _: LoginStorage.Delegate ->
            runtime.loginStorageDelegate = null
        }

        val usedHandled = GeckoResult<Void>()

        val user1 = "user1x"
        val pass1 = "pass1x"
        val guid = "test-guid"
        val origin = GeckoSessionTestRule.TEST_ENDPOINT
        val savedLogin = LoginEntry.Builder()
                .guid(guid)
                .origin(origin)
                .formActionOrigin(origin)
                .username(user1)
                .password(pass1)
                .build()
        val savedLogins = mutableListOf<LoginEntry>(savedLogin)

        sessionRule.addExternalDelegateUntilTestEnd(
                LoginStorage.Delegate::class, register, unregister,
                object : LoginStorage.Delegate {
            @AssertCalled
            override fun onLoginFetch(domain: String)
                    : GeckoResult<Array<LoginEntry>>? {
                assertThat("Domain should match", domain, equalTo("localhost"))

                return GeckoResult.fromValue(savedLogins.toTypedArray())
            }

            @AssertCalled(count = 1)
            override fun onLoginUsed(login: LoginEntry, usedFields: Int) {
                assertThat(
                    "Used fields should match",
                    usedFields,
                    equalTo(LoginStorage.UsedField.PASSWORD))

                assertThat(
                    "Username should match",
                    login.username,
                    equalTo(user1))

                assertThat(
                    "Password should match",
                    login.password,
                    equalTo(pass1))

                assertThat(
                    "GUID should match",
                    login.guid,
                    equalTo(guid))

                usedHandled.complete(null)
            }
        })

        sessionRule.delegateUntilTestEnd(object : Callbacks.PromptDelegate {
            @AssertCalled(false)
            override fun onLoginStoragePrompt(
                    session: GeckoSession,
                    prompt: LoginStoragePrompt)
                    : GeckoResult<PromptDelegate.PromptResponse>? {
                return null
            }
        })

        mainSession.loadTestPath(FORMS3_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.evaluateJS("document.querySelector('#form1').submit()")

        sessionRule.waitForResult(usedHandled)
    }
}
