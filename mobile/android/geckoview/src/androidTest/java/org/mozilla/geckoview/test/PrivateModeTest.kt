/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSessionSettings

@RunWith(AndroidJUnit4::class)
@MediumTest
class PrivateModeTest : BaseSessionTest() {
    @Test
    fun privateDataNotShared() {
        mainSession.loadUri("https://example.com")
        mainSession.waitForPageStop()

        mainSession.evaluateJS(
            """
            localStorage.setItem('ctx', 'regular');
        """,
        )

        val privateSession = sessionRule.createOpenSession(
            GeckoSessionSettings.Builder(mainSession.settings)
                .usePrivateMode(true)
                .build(),
        )
        privateSession.loadUri("https://example.com")
        privateSession.waitForPageStop()
        var localStorage = privateSession.evaluateJS(
            """
            localStorage.getItem('ctx') || 'null'
        """,
        ) as String

        // Ensure that the regular session's data hasn't leaked into the private session.
        assertThat(
            "Private mode local storage value should be empty",
            localStorage,
            Matchers.equalTo("null"),
        )

        privateSession.evaluateJS(
            """
            localStorage.setItem('ctx', 'private');
        """,
        )

        localStorage = mainSession.evaluateJS(
            """
            localStorage.getItem('ctx') || 'null'
        """,
        ) as String

        // Conversely, ensure private data hasn't leaked into the regular session.
        assertThat(
            "Regular mode storage value should be unchanged",
            localStorage,
            Matchers.equalTo("regular"),
        )
    }

    @Test
    fun privateModeStorageShared() {
        // Two private mode sessions should share the same storage (bug 1533406).
        val privateSession1 = sessionRule.createOpenSession(
            GeckoSessionSettings.Builder(mainSession.settings)
                .usePrivateMode(true)
                .build(),
        )
        privateSession1.loadUri("https://example.com")
        privateSession1.waitForPageStop()

        privateSession1.evaluateJS(
            """
            localStorage.setItem('ctx', 'private');
        """,
        )

        val privateSession2 = sessionRule.createOpenSession(
            GeckoSessionSettings.Builder(mainSession.settings)
                .usePrivateMode(true)
                .build(),
        )
        privateSession2.loadUri("https://example.com")
        privateSession2.waitForPageStop()

        val localStorage = privateSession2.evaluateJS(
            """
            localStorage.getItem('ctx') || 'null'
        """,
        ) as String

        assertThat(
            "Private mode storage value still set",
            localStorage,
            Matchers.equalTo("private"),
        )
    }
}
