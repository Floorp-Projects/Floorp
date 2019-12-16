/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4

import org.hamcrest.Matchers.*

import org.junit.Test
import org.junit.runner.RunWith

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.LoginStorage
import org.mozilla.geckoview.LoginStorage.LoginEntry
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay


@RunWith(AndroidJUnit4::class)
@MediumTest
class LoginStorageDelegateTest : BaseSessionTest() {

    @Test fun fetchLogins() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "signon.rememberSignons" to true,
                "signon.autofillForms.http" to true))

        val runtime = sessionRule.runtime;
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
                    assertThat(
                        "Domain should match",
                        domain,
                        equalTo("localhost"))

                    fetchHandled.complete(null);
                    return null
                }
            })

        mainSession.loadTestPath(FORMS_HTML_PATH)
        sessionRule.waitForResult(fetchHandled)
    }
}
