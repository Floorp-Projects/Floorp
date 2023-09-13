/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.extension

import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.action.WebExtensionAction.UpdatePromptRequestWebExtensionAction
import mozilla.components.browser.state.state.extension.WebExtensionPromptRequest
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionInstallException
import mozilla.components.support.ktx.android.content.appVersionName
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class WebExtensionPromptFeatureTest {

    private lateinit var webExtensionPromptFeature: WebExtensionPromptFeature
    private lateinit var store: BrowserStore

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        store = BrowserStore()
        webExtensionPromptFeature = spyk(
            WebExtensionPromptFeature(
                store = store,
                context = testContext,
                fragmentManager = mockk(relaxed = true),
            ),
        )
    }

    @Test
    fun `WHEN InstallationFailed is dispatched THEN handleInstallationFailedRequest is called`() {
        webExtensionPromptFeature.start()

        every { webExtensionPromptFeature.handleInstallationFailedRequest(any()) } just runs

        store.dispatch(
            UpdatePromptRequestWebExtensionAction(
                WebExtensionPromptRequest.BeforeInstallation.InstallationFailed(
                    mockk(),
                    mockk(),
                ),
            ),
        ).joinBlocking()

        verify { webExtensionPromptFeature.handleInstallationFailedRequest(any()) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with network error THEN showDialog with the correct message`() {
        val expectedTitle =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val exception = WebExtensionInstallException.NetworkFailure(
            extensionName = "name",
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(
                R.string.mozac_feature_addons_failed_to_install_network_error,
                "name",
            )

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with Blocklisted error THEN showDialog with the correct message`() {
        val expectedTitle =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val extensionName = "extensionName"
        val exception = WebExtensionInstallException.Blocklisted(
            extensionName = extensionName,
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_blocklisted_1, extensionName)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with UserCancelled error THEN do not showDialog`() {
        val expectedTitle = ""
        val extensionName = "extensionName"
        val exception = WebExtensionInstallException.UserCancelled(
            extensionName = extensionName,
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, extensionName)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify(exactly = 0) { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with Unknown error THEN showDialog with the correct message`() {
        val expectedTitle = ""
        val extensionName = "extensionName"
        val exception = WebExtensionInstallException.Unknown(
            extensionName = extensionName,
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, extensionName)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with Unknown error and no extension name THEN showDialog with the correct message`() {
        val expectedTitle = ""
        val exception = WebExtensionInstallException.Unknown(
            extensionName = null,
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install_generic)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with CorruptFile error THEN showDialog with the correct message`() {
        val expectedTitle =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val exception = WebExtensionInstallException.CorruptFile(
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install_corrupt_error)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with NotSigned error THEN showDialog with the correct message`() {
        val expectedTitle =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val exception = WebExtensionInstallException.NotSigned(
            throwable = Exception(),
        )
        val expectedMessage =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install_not_signed_error)

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }

    @Test
    fun `WHEN calling handleInstallationFailedRequest with Incompatible error THEN showDialog with the correct message`() {
        val expectedTitle =
            testContext.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val extensionName = "extensionName"
        val exception = WebExtensionInstallException.Incompatible(
            extensionName = extensionName,
            throwable = Exception(),
        )
        val appName = testContext.getString(R.string.app_name)
        val version = testContext.appVersionName
        val expectedMessage =
            testContext.getString(
                R.string.mozac_feature_addons_failed_to_install_incompatible_error,
                extensionName,
                appName,
                version,
            )

        webExtensionPromptFeature.handleInstallationFailedRequest(
            exception = exception,
        )

        verify { webExtensionPromptFeature.showDialog(expectedTitle, expectedMessage) }
    }
}
