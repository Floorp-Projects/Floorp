/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TopSitesUseCasesTest {

    @Test
    fun `AddPinnedSiteUseCase`() = runBlocking {
        val topSitesStorage: TopSitesStorage = mock()
        val useCases = TopSitesUseCases(topSitesStorage)

        useCases.addPinnedSites("Mozilla", "https://www.mozilla.org", isDefault = true)
        verify(topSitesStorage).addPinnedSite(
            "Mozilla",
            "https://www.mozilla.org",
            isDefault = true
        )
    }

    @Test
    fun `RemoveTopSiteUseCase`() = runBlocking {
        val topSitesStorage: TopSitesStorage = mock()
        val topSite: TopSite = mock()
        val useCases = TopSitesUseCases(topSitesStorage)

        useCases.removeTopSites(topSite)

        verify(topSitesStorage).removeTopSite(topSite)
    }
}
