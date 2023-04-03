/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.cookiebanners

import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.PreferenceDataStoreFactory
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.preferencesDataStoreFile
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ReportSiteDomainsRepositoryTest {

    companion object {
        const val TEST_DATASTORE_NAME = "test_data_store"
    }

    private lateinit var testDataStore: DataStore<Preferences>

    private lateinit var reportSiteDomainsRepository: ReportSiteDomainsRepository

    @Before
    fun setUp() {
        testDataStore = PreferenceDataStoreFactory.create(
            produceFile = { testContext.preferencesDataStoreFile(TEST_DATASTORE_NAME) },
        )
        reportSiteDomainsRepository = ReportSiteDomainsRepository(testDataStore)
    }

    @After
    fun cleanUp() = runTest { testDataStore.edit { it.clear() } }

    @Test
    fun `GIVEN site domain url WHEN site domain url is not saved THEN is side domain reported return false`() =
        runTest {
            assertFalse(reportSiteDomainsRepository.isSiteDomainReported("mozilla.org"))
        }

    @Test
    fun `GIVEN site domain url WHEN site domain url is saved THEN is side domain reported return true`() =
        runTest {
            val siteDomainReported = "mozilla.org"

            reportSiteDomainsRepository.saveSiteDomain(siteDomainReported)

            assertTrue(reportSiteDomainsRepository.isSiteDomainReported(siteDomainReported))
        }

    @Test
    fun `GIVEN site domain urls WHEN site domain urls are saved THEN is side domain reported return true for each one`() =
        runTest {
            val mozillaSiteDomainReported = "mozilla.org"
            val youtubeSiteDomainReported = "youtube.com"

            reportSiteDomainsRepository.saveSiteDomain(mozillaSiteDomainReported)
            reportSiteDomainsRepository.saveSiteDomain(youtubeSiteDomainReported)

            assertTrue(reportSiteDomainsRepository.isSiteDomainReported(mozillaSiteDomainReported))
            assertTrue(reportSiteDomainsRepository.isSiteDomainReported(youtubeSiteDomainReported))
        }
}
