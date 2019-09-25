/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import android.graphics.Bitmap
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

class ContentActionTest {
    private lateinit var store: BrowserStore
    private lateinit var tabId: String
    private lateinit var otherTabId: String

    private val tab: TabSessionState
        get() = store.state.tabs.find { it.id == tabId }!!

    private val otherTab: TabSessionState
        get() = store.state.tabs.find { it.id == otherTabId }!!

    @Before
    fun setUp() {
        val state = BrowserState(tabs = listOf(
            createTab(url = "https://www.mozilla.org").also {
                tabId = it.id
            },
            createTab(url = "https://www.firefox.com").also {
                otherTabId = it.id
            }
        ))

        store = BrowserStore(state)
    }

    @Test
    fun `UpdateUrlAction updates URL`() {
        val newUrl = "https://www.example.org"

        assertNotEquals(newUrl, tab.content.url)
        assertNotEquals(newUrl, otherTab.content.url)

        store.dispatch(
            ContentAction.UpdateUrlAction(tab.id, newUrl)
        ).joinBlocking()

        assertEquals(newUrl, tab.content.url)
        assertNotEquals(newUrl, otherTab.content.url)
    }

    @Test
    fun `UpdateLoadingStateAction updates loading state`() {
        assertFalse(tab.content.loading)
        assertFalse(otherTab.content.loading)

        store.dispatch(
            ContentAction.UpdateLoadingStateAction(tab.id, true)
        ).joinBlocking()

        assertTrue(tab.content.loading)
        assertFalse(otherTab.content.loading)

        store.dispatch(
            ContentAction.UpdateLoadingStateAction(tab.id, false)
        ).joinBlocking()

        assertFalse(tab.content.loading)
        assertFalse(otherTab.content.loading)

        store.dispatch(
            ContentAction.UpdateLoadingStateAction(tab.id, true)
        ).joinBlocking()

        store.dispatch(
            ContentAction.UpdateLoadingStateAction(otherTab.id, true)
        ).joinBlocking()

        assertTrue(tab.content.loading)
        assertTrue(otherTab.content.loading)
    }

    @Test
    fun `UpdateTitleAction updates title`() {
        val newTitle = "This is a title"

        assertNotEquals(newTitle, tab.content.title)
        assertNotEquals(newTitle, otherTab.content.title)

        store.dispatch(
            ContentAction.UpdateTitleAction(tab.id, newTitle)
        ).joinBlocking()

        assertEquals(newTitle, tab.content.title)
        assertNotEquals(newTitle, otherTab.content.title)
    }

    @Test
    fun `UpdateProgressAction updates progress`() {
        assertEquals(0, tab.content.progress)
        assertEquals(0, otherTab.content.progress)

        store.dispatch(ContentAction.UpdateProgressAction(tab.id, 75)).joinBlocking()

        assertEquals(75, tab.content.progress)
        assertEquals(0, otherTab.content.progress)

        store.dispatch(ContentAction.UpdateProgressAction(otherTab.id, 25)).joinBlocking()
        store.dispatch(ContentAction.UpdateProgressAction(tab.id, 85)).joinBlocking()

        assertEquals(85, tab.content.progress)
        assertEquals(25, otherTab.content.progress)
    }

    @Test
    fun `UpdateSearchTermsAction updates URL`() {
        val searchTerms = "Hello World"

        assertNotEquals(searchTerms, tab.content.searchTerms)
        assertNotEquals(searchTerms, otherTab.content.searchTerms)

        store.dispatch(
            ContentAction.UpdateSearchTermsAction(tab.id, searchTerms)
        ).joinBlocking()

        assertEquals(searchTerms, tab.content.searchTerms)
        assertNotEquals(searchTerms, otherTab.content.searchTerms)
    }

    @Test
    fun `UpdateSecurityInfo updates searchInfo`() {
        val newSecurityInfo = SecurityInfoState(true, "mozilla.org", "The Mozilla Team")

        assertNotEquals(newSecurityInfo, tab.content.securityInfo)
        assertNotEquals(newSecurityInfo, otherTab.content.securityInfo)

        store.dispatch(
            ContentAction.UpdateSecurityInfoAction(tab.id, newSecurityInfo)
        ).joinBlocking()

        assertEquals(newSecurityInfo, tab.content.securityInfo)
        assertNotEquals(newSecurityInfo, otherTab.content.securityInfo)

        assertEquals(true, tab.content.securityInfo.secure)
        assertEquals("mozilla.org", tab.content.securityInfo.host)
        assertEquals("The Mozilla Team", tab.content.securityInfo.issuer)
    }

    @Test
    fun `UpdateThumbnailAction updates thumbnail`() {
        val thumbnail = spy(Bitmap::class.java)

        assertNotEquals(thumbnail, tab.content.thumbnail)
        assertNotEquals(thumbnail, otherTab.content.thumbnail)

        store.dispatch(
                ContentAction.UpdateThumbnailAction(tab.id, thumbnail)
        ).joinBlocking()

        assertEquals(thumbnail, tab.content.thumbnail)
        assertNotEquals(thumbnail, otherTab.content.thumbnail)
    }

    @Test
    fun `RemoveThumbnailAction removes thumbnail`() {
        val thumbnail = spy(Bitmap::class.java)

        assertNotEquals(thumbnail, tab.content.thumbnail)

        store.dispatch(
                ContentAction.UpdateThumbnailAction(tab.id, thumbnail)
        ).joinBlocking()

        assertEquals(thumbnail, tab.content.thumbnail)

        store.dispatch(
                ContentAction.RemoveThumbnailAction(tab.id)
        ).joinBlocking()

        assertNull(tab.content.thumbnail)
    }

    @Test
    fun `UpdateIconAction updates icon`() {
        val icon = spy(Bitmap::class.java)

        assertNotEquals(icon, tab.content.icon)
        assertNotEquals(icon, otherTab.content.icon)

        store.dispatch(
                ContentAction.UpdateIconAction(tab.id, icon)
        ).joinBlocking()

        assertEquals(icon, tab.content.icon)
        assertNotEquals(icon, otherTab.content.icon)
    }

    @Test
    fun `RemoveIconAction removes icon`() {
        val icon = spy(Bitmap::class.java)

        assertNotEquals(icon, tab.content.icon)

        store.dispatch(
                ContentAction.UpdateIconAction(tab.id, icon)
        ).joinBlocking()

        assertEquals(icon, tab.content.icon)

        store.dispatch(
                ContentAction.RemoveIconAction(tab.id)
        ).joinBlocking()

        assertNull(tab.content.icon)
    }

    @Test
    fun `Updating custom tab`() {
        val customTab = createCustomTab("https://getpocket.com")
        val otherCustomTab = createCustomTab("https://www.google.com")

        store.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
        store.dispatch(CustomTabListAction.AddCustomTabAction(otherCustomTab)).joinBlocking()

        store.dispatch(ContentAction.UpdateUrlAction(customTab.id, "https://www.example.org")).joinBlocking()
        store.dispatch(ContentAction.UpdateTitleAction(customTab.id, "I am a custom tab")).joinBlocking()

        val updatedCustomTab = store.state.findCustomTab(customTab.id)!!
        val updatedOtherCustomTab = store.state.findCustomTab(otherCustomTab.id)!!

        assertEquals("https://www.example.org", updatedCustomTab.content.url)
        assertNotEquals("https://www.example.org", updatedOtherCustomTab.content.url)
        assertNotEquals("https://www.example.org", tab.content.url)
        assertNotEquals("https://www.example.org", otherTab.content.url)

        assertEquals("I am a custom tab", updatedCustomTab.content.title)
        assertNotEquals("I am a custom tab", updatedOtherCustomTab.content.title)
        assertNotEquals("I am a custom tab", tab.content.title)
        assertNotEquals("I am a custom tab", otherTab.content.title)
    }

    @Test
    fun `UpdateDownloadAction updates download`() {
        assertNull(tab.content.download)

        val download1: DownloadState = mock()

        store.dispatch(
            ContentAction.UpdateDownloadAction(tab.id, download1)
        ).joinBlocking()

        assertEquals(download1, tab.content.download)

        val download2: DownloadState = mock()

        store.dispatch(
            ContentAction.UpdateDownloadAction(tab.id, download2)
        ).joinBlocking()

        assertEquals(download2, tab.content.download)
    }

    @Test
    fun `ConsumeDownloadAction removes download`() {
        val download: DownloadState = mock()
        doReturn("1337").`when`(download).id

        store.dispatch(
            ContentAction.UpdateDownloadAction(tab.id, download)
        ).joinBlocking()

        assertEquals(download, tab.content.download)

        store.dispatch(
            ContentAction.ConsumeDownloadAction(tab.id, downloadId = "1337")
        ).joinBlocking()

        assertNull(tab.content.download)
    }

    @Test
    fun `ConsumeDownloadAction does not remove download with different id`() {
        val download: DownloadState = mock()
        doReturn("1337").`when`(download).id

        store.dispatch(
            ContentAction.UpdateDownloadAction(tab.id, download)
        ).joinBlocking()

        assertEquals(download, tab.content.download)

        store.dispatch(
            ContentAction.ConsumeDownloadAction(tab.id, downloadId = "4223")
        ).joinBlocking()

        assertNotNull(tab.content.download)
    }

    @Test
    fun `UpdateHitResultAction updates hit result`() {
        assertNull(tab.content.hitResult)

        val hitResult1: HitResult = mock()

        store.dispatch(
            ContentAction.UpdateHitResultAction(tab.id, hitResult1)
        ).joinBlocking()

        assertEquals(hitResult1, tab.content.hitResult)

        val hitResult2: HitResult = mock()

        store.dispatch(
            ContentAction.UpdateHitResultAction(tab.id, hitResult2)
        ).joinBlocking()

        assertEquals(hitResult2, tab.content.hitResult)
    }

    @Test
    fun `ConsumeHitResultAction removes hit result`() {
        val hitResult: HitResult = mock()

        store.dispatch(
            ContentAction.UpdateHitResultAction(tab.id, hitResult)
        ).joinBlocking()

        assertEquals(hitResult, tab.content.hitResult)

        store.dispatch(
            ContentAction.ConsumeHitResultAction(tab.id)
        ).joinBlocking()

        assertNull(tab.content.hitResult)
    }

    @Test
    fun `UpdatePromptRequestAction updates request`() {
        assertNull(tab.content.promptRequest)

        val promptRequest1: PromptRequest = mock()

        store.dispatch(
            ContentAction.UpdatePromptRequestAction(tab.id, promptRequest1)
        ).joinBlocking()

        assertEquals(promptRequest1, tab.content.promptRequest)

        val promptRequest2: PromptRequest = mock()

        store.dispatch(
            ContentAction.UpdatePromptRequestAction(tab.id, promptRequest2)
        ).joinBlocking()

        assertEquals(promptRequest2, tab.content.promptRequest)
    }

    @Test
    fun `ConsumePromptRequestAction removes result`() {
        val promptRequest: PromptRequest = mock()

        store.dispatch(
            ContentAction.UpdatePromptRequestAction(tab.id, promptRequest)
        ).joinBlocking()

        assertEquals(promptRequest, tab.content.promptRequest)

        store.dispatch(
            ContentAction.ConsumePromptRequestAction(tab.id)
        ).joinBlocking()

        assertNull(tab.content.promptRequest)
    }

    @Test
    fun `AddFindResultAction adds result`() {
        assertTrue(tab.content.findResults.isEmpty())

        val result: FindResultState = mock()
        store.dispatch(
            ContentAction.AddFindResultAction(tab.id, result)
        ).joinBlocking()

        assertEquals(1, tab.content.findResults.size)
        assertEquals(result, tab.content.findResults.last())

        val result2: FindResultState = mock()
        store.dispatch(
            ContentAction.AddFindResultAction(tab.id, result2)
        ).joinBlocking()

        assertEquals(2, tab.content.findResults.size)
        assertEquals(result2, tab.content.findResults.last())
    }

    @Test
    fun `ClearFindResultsAction removes all results`() {
        store.dispatch(
            ContentAction.AddFindResultAction(tab.id, mock())
        ).joinBlocking()

        store.dispatch(
            ContentAction.AddFindResultAction(tab.id, mock())
        ).joinBlocking()

        assertEquals(2, tab.content.findResults.size)

        store.dispatch(
            ContentAction.ClearFindResultsAction(tab.id)
        ).joinBlocking()

        assertTrue(tab.content.findResults.isEmpty())
    }
}