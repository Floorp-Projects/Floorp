/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.verify
import java.io.IOException

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContileTopSitesUseCasesTest {

    @Test
    fun `WHEN refresh contile top site use case is called THEN call the provider to fetch top sites bypassing the cache`() = runTest {
        val provider: ContileTopSitesProvider = mock()

        ContileTopSitesUseCases.initialize(provider)

        whenever(provider.getTopSites(anyBoolean())).thenReturn(emptyList())

        ContileTopSitesUseCases().refreshContileTopSites.invoke()

        verify(provider).getTopSites(eq(false))

        Unit
    }

    @Test(expected = IOException::class)
    fun `GIVEN the provider fails to fetch contile top sites WHEN refresh contile top site use case is called THEN an exception is thrown`() = runTest {
        val provider: ContileTopSitesProvider = mock()
        val throwable = IOException("test")

        ContileTopSitesUseCases.initialize(provider)

        whenever(provider.getTopSites(anyBoolean())).then {
            throw throwable
        }

        ContileTopSitesUseCases().refreshContileTopSites.invoke()
    }
}
