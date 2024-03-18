/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.ext.toLocalSpoc
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.spocs.db.SpocImpressionEntity
import mozilla.components.service.pocket.spocs.db.SpocsDao
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
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
    }

    @Test
    fun `GIVEN SpocsRepository WHEN asking for all spocs THEN return db entities mapped to domain type`() = runTest {
        val spoc = PocketTestResources.dbExpectedPocketSpoc
        val impressions = listOf(
            SpocImpressionEntity(spoc.id),
            SpocImpressionEntity(333),
            SpocImpressionEntity(spoc.id),
        )
        doReturn(listOf(spoc)).`when`(dao).getAllSpocs()
        doReturn(impressions).`when`(dao).getSpocsImpressions()

        val result = spocsRepo.getAllSpocs()

        verify(dao).getAllSpocs()
        assertEquals(1, result.size)
        assertSame(spoc.title, result[0].title)
        assertSame(spoc.url, result[0].url)
        assertSame(spoc.imageUrl, result[0].imageUrl)
        assertSame(spoc.impressionShim, result[0].shim.impression)
        assertSame(spoc.clickShim, result[0].shim.click)
        assertEquals(spoc.priority, result[0].priority)
        assertEquals(2, result[0].caps.currentImpressions.size)
        assertEquals(spoc.lifetimeCapCount, result[0].caps.lifetimeCount)
        assertEquals(spoc.flightCapCount, result[0].caps.flightCount)
        assertEquals(spoc.flightCapPeriod, result[0].caps.flightPeriod)
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

    @Test
    fun `GIVEN SpocsRepository WHEN recording new spocs impressions THEN add this to the database`() = runTest {
        val spocsIds = listOf(3, 33, 444)
        val impressionsCaptor = argumentCaptor<List<SpocImpressionEntity>>()

        spocsRepo.recordImpressions(spocsIds)

        verify(dao).recordImpressions(impressionsCaptor.capture())
        assertEquals(spocsIds.size, impressionsCaptor.value.size)
        assertEquals(spocsIds[0], impressionsCaptor.value[0].spocId)
        assertEquals(spocsIds[1], impressionsCaptor.value[1].spocId)
        assertEquals(spocsIds[2], impressionsCaptor.value[2].spocId)
    }
}
