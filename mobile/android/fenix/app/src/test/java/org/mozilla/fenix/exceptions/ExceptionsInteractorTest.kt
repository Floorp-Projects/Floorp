/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Test

class ExceptionsInteractorTest {

    @Test
    fun onLearnMore() {
        var learnMoreClicked = false
        val interactor = ExceptionsInteractor(
            { learnMoreClicked = true },
            mockk(),
            mockk()
        )
        interactor.onLearnMore()
        assertEquals(true, learnMoreClicked)
    }

    @Test
    fun onDeleteAll() {
        var onDeleteAll = false
        val interactor = ExceptionsInteractor(
            mockk(),
            mockk(),
            { onDeleteAll = true }
        )
        interactor.onDeleteAll()
        assertEquals(true, onDeleteAll)
    }

    @Test
    fun onDeleteOne() {
        var exceptionsItemReceived: ExceptionsItem? = null
        val exceptionsItem = ExceptionsItem("url")
        val interactor = ExceptionsInteractor(
            mockk(),
            { exceptionsItemReceived = exceptionsItem },
            mockk()
        )
        interactor.onDeleteOne(exceptionsItem)
        assertEquals(exceptionsItemReceived, exceptionsItem)
    }
}
