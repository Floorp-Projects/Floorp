/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.core.IsEqual.equalTo
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class WebExtensionTest : BaseSessionTest() {
    @Test
    @WithDevToolsAPI
    fun registerWebExtension() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        val colorBefore = sessionRule.evaluateJS(mainSession, "document.body.style.borderColor")
        assertThat("The border color should be empty when loading without extensions.",
                colorBefore as String, equalTo(""))

        // Load the WebExtension that will add a border to the body
        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(
                WebExtension("resource://android/assets/web_extensions/borderify/")
        ))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        val color = sessionRule.evaluateJS(mainSession, "document.body.style.borderColor")
        assertThat("Content script should have been applied",
                color as String, equalTo("red"))
    }

    @Test
    fun badFileType() {
        testRegisterError("resource://android/bad/location/error",
                "does not point to a folder or an .xpi")
    }

    @Test
    fun badLocationXpi() {
        testRegisterError("resource://android/bad/location/error.xpi",
                "NS_ERROR_FILE_NOT_FOUND")
    }

    @Test
    fun badLocationFolder() {
        testRegisterError("resource://android/bad/location/error/",
                "NS_ERROR_FILE_NOT_FOUND")
    }

    private fun testRegisterError(location: String, expectedError: String) {
        try {
            sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(
                    WebExtension(location)
            ))
        } catch (ex: Exception) {
            // Let's make sure the error message contains the WebExtension URL
            assertTrue(ex.message!!.contains(location))

            // and it contains the expected error message
            assertTrue(ex.message!!.contains(expectedError))

            return
        }

        fail("The above code should throw.")
    }
}
