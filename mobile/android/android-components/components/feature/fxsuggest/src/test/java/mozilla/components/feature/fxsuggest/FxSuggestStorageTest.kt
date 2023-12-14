/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.appservices.suggest.SuggestApiException
import mozilla.appservices.suggest.SuggestStore
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionQuery
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FxSuggestStorageTest {
    @Test
    fun `GIVEN an exception is thrown WHEN querying the store THEN the exception should be reported`() = runTest {
        val store: SuggestStore = mock()
        whenever(store.query(any())).then {
            throw SuggestApiException.Other("Mercury in retrograde")
        }

        val crashReporter: CrashReporting = mock()
        val storage = spy(FxSuggestStorage(testContext, crashReporter))
        whenever(storage.store).thenReturn(lazy { store })

        val suggestions = storage.query(SuggestionQuery("la", providers = emptyList()))
        assertTrue(suggestions.isEmpty())
        verify(crashReporter).submitCaughtException(any())
    }

    @Test
    fun `GIVEN an exception is not thrown WHEN querying the store THEN nothing should be reported`() = runTest {
        val store: SuggestStore = mock()
        whenever(store.query(any())).thenReturn(
            listOf(
                Suggestion.Wikipedia(
                    title = "Las Vegas",
                    url = "https://wikipedia.org/wiki/Las_Vegas",
                    icon = null,
                    fullKeyword = "las",
                ),
            ),
        )

        val crashReporter: CrashReporting = mock()
        val storage = spy(FxSuggestStorage(testContext, crashReporter))
        whenever(storage.store).thenReturn(lazy { store })

        val suggestions = storage.query(SuggestionQuery("la", providers = emptyList()))
        assertEquals(1, suggestions.size)
        verify(crashReporter, never()).submitCaughtException(any())
    }

    @Test
    fun `GIVEN an exception is thrown WHEN ingesting THEN the exception should be reported`() = runTest {
        val crashReporter: CrashReporting = mock()
        val store: SuggestStore = mock()
        whenever(store.ingest(any())).then {
            throw SuggestApiException.Other("Mercury in retrograde")
        }

        val storage = spy(FxSuggestStorage(testContext, crashReporter))
        whenever(storage.store).thenReturn(lazy { store })

        val success = storage.ingest()
        assertFalse(success)
        verify(crashReporter).submitCaughtException(any())
    }

    @Test
    fun `GIVEN an exception is not thrown WHEN ingesting THEN nothing should be reported`() = runTest {
        val crashReporter: CrashReporting = mock()
        val store: SuggestStore = mock()
        doNothing().`when`(store).ingest(any())

        val storage = spy(FxSuggestStorage(testContext, crashReporter))
        whenever(storage.store).thenReturn(lazy { store })

        val success = storage.ingest()
        assertTrue(success)
        verify(crashReporter, never()).submitCaughtException(any())
    }
}
