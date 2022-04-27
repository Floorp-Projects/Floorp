/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.ext.toLocalSpoc
import mozilla.components.service.pocket.ext.toPocketSponsoredStory
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.spocs.db.SpocsDao
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class SpocsRepositoryTest {
    private val spocsRepo = spy(SpocsRepository(testContext))
    private val dao = mock(SpocsDao::class.java)

    @Before
    fun setUp() {
        doReturn(dao).`when`(spocsRepo).spocsDao
        Mockito.`when`(spocsRepo.spocsDao).thenReturn(dao)
    }

    @Test
    fun `GIVEN SpocsRepository WHEN asking for all spocs THEN return db entities mapped to domain type`() = runTest {
        val spoc = PocketTestResources.dbExpectedPocketSpoc
        doReturn(listOf(spoc)).`when`(dao).getAllSpocs()

        val result = spocsRepo.getAllSpocs()

        verify(dao).getAllSpocs()
        assertEquals(1, result.size)
        assertEquals(spoc.toPocketSponsoredStory(), result[0])
    }

    @Test
    fun `GIVEN SpocsRepository WHEN asking to delete all spocs THEN delete all from the database`() = runTest {
        spocsRepo.deleteAllSpocs()

        verify(dao).deleteAllSpocs()
    }

    @Test
    fun `GIVEN SpocsRepository WHEN adding a new list of spocs THEN replace all present in the database`() = runTest {
        val spoc = PocketTestResources.apiExpectedPocketSpocs[0]

        spocsRepo.addSpocs(listOf(spoc))

        verify(dao).cleanOldAndInsertNewSpocs(listOf(spoc.toLocalSpoc()))
    }
}
