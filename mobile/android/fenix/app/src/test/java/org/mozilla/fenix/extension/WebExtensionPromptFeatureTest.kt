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
import mozilla.components.browser.state.state.extension.WebExtensionPromptRequest.Permissions
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
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
                provideAddons = { emptyList() },
                context = testContext,
                snackBarParentView = mockk(relaxed = true),
                fragmentManager = mockk(relaxed = true),
            ),
        )
    }

    @Test
    fun `WHEN add-on is not found THEN unsupported error message is shown`() {
        val onConfirm = mockk<(Boolean) -> Unit>(relaxed = true)
        every { webExtensionPromptFeature.consumePromptRequest() } just runs
        every { webExtensionPromptFeature.showUnsupportedError() } returns mockk()

        webExtensionPromptFeature.start()

        // Passing a mocked WebExtension instance here will result in no add-on being found.
        store.dispatch(UpdatePromptRequestWebExtensionAction(Permissions(mockk(), onConfirm))).joinBlocking()

        verify { onConfirm(false) }
        verify { webExtensionPromptFeature.consumePromptRequest() }
        verify { webExtensionPromptFeature.showUnsupportedError() }
    }
}
