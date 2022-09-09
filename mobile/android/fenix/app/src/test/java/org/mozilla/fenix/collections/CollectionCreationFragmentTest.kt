/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.createAddedTestFragment
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.perf.TestStrictModeManager

private const val URL_MOZILLA = "www.mozilla.org"
private const val SESSION_ID_MOZILLA = "0"
private const val URL_BCC = "www.bcc.co.uk"
private const val SESSION_ID_BCC = "1"

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationFragmentTest {

    private val publicSuffixList = mockk<PublicSuffixList>(relaxed = true)

    private val sessionMozilla = createTab(URL_MOZILLA, id = SESSION_ID_MOZILLA)
    private val sessionBcc = createTab(URL_BCC, id = SESSION_ID_BCC)
    private val state = BrowserState(
        tabs = listOf(sessionMozilla, sessionBcc),
    )

    @Before
    fun before() {
        MockKAnnotations.init(this)
        every { publicSuffixList.stripPublicSuffix(URL_MOZILLA) } returns CompletableDeferred(URL_MOZILLA)
        every { publicSuffixList.stripPublicSuffix(URL_BCC) } returns CompletableDeferred(URL_BCC)
        every { testContext.components.publicSuffixList } returns publicSuffixList
    }

    @Test
    fun `creation dialog shows and can be dismissed`() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        every { testContext.components.core.store } returns BrowserStore(state)
        every { testContext.components.core.tabCollectionStorage } returns TabCollectionStorage(
            testContext,
            TestStrictModeManager(),
        )
        val fragment = createAddedTestFragment {
            CollectionCreationFragment().apply {
                arguments = CollectionCreationFragmentArgs(
                    saveCollectionStep = SaveCollectionStep.SelectTabs,
                ).toBundle()
            }
        }

        assertNotNull(fragment.dialog)
        assertTrue(fragment.requireDialog().isShowing)
    }
}
