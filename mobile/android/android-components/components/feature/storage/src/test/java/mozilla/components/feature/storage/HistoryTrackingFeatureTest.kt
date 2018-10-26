/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.storage

import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.never

class HistoryTrackingFeatureTest {
    @Test
    fun `feature sets a history delegate on the engine`() {
        val engine: Engine = mock()
        val settings = DefaultSettings()
        `when`(engine.settings).thenReturn(settings)

        assertNull(settings.historyTrackingDelegate)
        HistoryTrackingFeature(engine, mock())
        assertNotNull(settings.historyTrackingDelegate)
    }

    @Test
    fun `history delegate doesn't interact with storage in private mode`() {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onVisited("http://www.mozilla.org", false, true)
        verify(storage, never()).recordVisit(any(), any())

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla", true)
        verify(storage, never()).recordObservation(any(), any())

        val systemCallback = mock<(List<String>) -> Unit>()
        delegate.getVisited(systemCallback, true)
        verify(systemCallback).invoke(listOf())
        verify(storage, never()).getVisited(systemCallback)

        val geckoCallback = mock<(List<Boolean>) -> Unit>()
        delegate.getVisited(listOf("http://www.mozilla.com"), geckoCallback, true)
        verify(geckoCallback).invoke(listOf(false))
        verify(storage, never()).getVisited(listOf("http://www.mozilla.com"), geckoCallback)

        delegate.getVisited(listOf("http://www.mozilla.com", "http://www.firefox.com"), geckoCallback, true)
        verify(geckoCallback).invoke(listOf(false, false))
        verify(storage, never()).getVisited(listOf("http://www.mozilla.com", "http://www.firefox.com"), geckoCallback)
    }

    @Test
    fun `history delegate passes through onVisited calls`() {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onVisited("http://www.mozilla.org", false, false)
        verify(storage).recordVisit("http://www.mozilla.org", VisitType.LINK)

        delegate.onVisited("http://www.firefox.com", true, false)
        verify(storage).recordVisit("http://www.firefox.com", VisitType.RELOAD)
    }

    @Test
    fun `history delegate passes through onTitleChanged calls`() {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla", false)
        verify(storage).recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
    }

    @Test
    fun `history delegate passes through getVisited calls`() {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        val systemCallback = mock<(List<String>) -> Unit>()
        delegate.getVisited(systemCallback, false)
        verify(storage).getVisited(systemCallback)

        val geckoCallback = mock<(List<Boolean>) -> Unit>()
        delegate.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"), geckoCallback, false)
        verify(storage).getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"), geckoCallback)
    }
}