/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import kotlinx.coroutines.cancelChildren
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.appservices.places.uniffi.PlacesException
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify
import kotlin.coroutines.CoroutineContext

class PlacesStorageTest {
    private val storage = FakePlacesStorage()

    @Test
    fun `WHEN all reads are interrupted THEN no exception is thrown`() {
        doAnswer {
            throw PlacesException.OperationInterrupted("This should be caught")
        }.`when`(storage.reader).interrupt()

        storage.interruptCurrentReads()

        verify(storage.reader).interrupt()
    }

    @Test
    fun `WHEN all writes are interrupted THEN no exception is thrown`() {
        doAnswer {
            throw PlacesException.OperationInterrupted("This should be caught")
        }.`when`(storage.writer).interrupt()

        storage.interruptCurrentWrites()

        verify(storage.writer).interrupt()
    }

    @Test
    fun `WHEN a call is made to clean all reads THEN they are cancelled`() {
        storage.readScope = mock {
            doReturn(mock<CoroutineContext>()).`when`(this).coroutineContext
        }

        storage.cancelReads()

        verify(storage.reader).interrupt()
        verify(storage.readScope.coroutineContext).cancelChildren()
    }

    @Test
    fun `WHEN a call is made to clean all writes THEN they are cancelled`() {
        storage.writeScope = mock {
            doReturn(mock<CoroutineContext>()).`when`(this).coroutineContext
        }

        storage.cancelWrites()

        verify(storage.writer).interrupt()
        verify(storage.writeScope.coroutineContext).cancelChildren()
    }
}

class FakePlacesStorage(
    context: Context = mock(),
) : PlacesStorage(context) {
    override val logger = Logger("FakePlacesStorage")
    override fun registerWithSyncManager() {}

    override val writer: PlacesWriterConnection = mock()
    override val reader: PlacesReaderConnection = mock()
}
