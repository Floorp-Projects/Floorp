/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.async
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.createAddedTestFragment
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

private const val URL_MOZILLA = "www.mozilla.org"
private const val SESSION_ID_MOZILLA = "0"
private const val URL_BCC = "www.bcc.co.uk"
private const val SESSION_ID_BCC = "1"

private const val SESSION_ID_BAD_1 = "not a real session id"
private const val SESSION_ID_BAD_2 = "definitely not a real session id"

@ExperimentalCoroutinesApi
@ObsoleteCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationFragmentTest {

    @MockK private lateinit var sessionManager: SessionManager
    @MockK private lateinit var publicSuffixList: PublicSuffixList
    @MockK private lateinit var store: BrowserStore

    private val sessionMozilla = Session(initialUrl = URL_MOZILLA, id = SESSION_ID_MOZILLA)
    private val sessionBcc = Session(initialUrl = URL_BCC, id = SESSION_ID_BCC)

    @Before
    fun before() {
        MockKAnnotations.init(this)
        every { sessionManager.findSessionById(SESSION_ID_MOZILLA) } answers { sessionMozilla }
        every { sessionManager.findSessionById(SESSION_ID_BCC) } answers { sessionBcc }
        every { sessionManager.findSessionById(SESSION_ID_BAD_1) } answers { null }
        every { sessionManager.findSessionById(SESSION_ID_BAD_2) } answers { null }
        every { publicSuffixList.stripPublicSuffix(URL_MOZILLA) } answers { GlobalScope.async { URL_MOZILLA } }
        every { publicSuffixList.stripPublicSuffix(URL_BCC) } answers { GlobalScope.async { URL_BCC } }
        every { store.state } answers { BrowserState() }
    }

    @Test
    fun `creation dialog shows and can be dismissed`() {
        val fragment = createAddedTestFragment {
            CollectionCreationFragment().apply {
                arguments = CollectionCreationFragmentArgs(
                    saveCollectionStep = SaveCollectionStep.SelectTabs
                ).toBundle()
            }
        }

        assertNotNull(fragment.dialog)
        assertTrue(fragment.requireDialog().isShowing)
        fragment.dismiss()
        assertNull(fragment.dialog)
    }

    @Test
    fun `GIVEN tabs are present in session manager WHEN getTabs is called THEN tabs will be returned`() {
        val tabs = sessionManager
            .getTabs(arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BCC), store, publicSuffixList)

        val hosts = tabs.map { it.hostname }

        assertEquals(URL_MOZILLA, hosts[0])
        assertEquals(URL_BCC, hosts[1])
    }

    @Test
    fun `GIVEN some tabs are present in session manager WHEN getTabs is called THEN only valid tabs will be returned`() {
        val tabs = sessionManager
            .getTabs(arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BAD_1), store, publicSuffixList)

        val hosts = tabs.map { it.hostname }

        assertEquals(URL_MOZILLA, hosts[0])
        assertEquals(1, hosts.size)
    }

    @Test
    fun `GIVEN tabs are not present in session manager WHEN getTabs is called THEN an empty list will be returned`() {
        val tabs = sessionManager
            .getTabs(arrayOf(SESSION_ID_BAD_1, SESSION_ID_BAD_2), store, publicSuffixList)

        assertEquals(emptyList<Tab>(), tabs)
    }

    @Test
    fun `WHEN getTabs is called will null tabIds THEN an empty list will be returned`() {
        val tabs = sessionManager
            .getTabs(null, store, publicSuffixList)

        assertEquals(emptyList<Tab>(), tabs)
    }
}
