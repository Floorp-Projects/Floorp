/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class TrackingProtectionExceptionsFragmentStoreTest {
    @Test
    fun onChange() = runTest {
        val initialState = ExceptionsFragmentState()
        val store = ExceptionsFragmentStore(initialState)
        val newExceptionsItem = ExceptionItem("URL")

        store.dispatch(ExceptionsFragmentAction.Change(listOf(newExceptionsItem))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.items,
            listOf(newExceptionsItem),
        )
    }

    private data class ExceptionItem(override val url: String) : TrackingProtectionException
}
