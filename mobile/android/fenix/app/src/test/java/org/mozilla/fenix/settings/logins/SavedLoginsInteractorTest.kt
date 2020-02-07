/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

class SavedLoginsInteractorTest {

    @Test
    fun itemClicked() {
        val savedLoginClicked: (SavedLoginsItem) -> Unit = mockk(relaxed = true)
        val learnMore: () -> Unit = mockk(relaxed = true)
        val interactor = SavedLoginsInteractor(
            savedLoginClicked,
            learnMore
        )

        val item = SavedLoginsItem("mozilla.org", "username", "password", "id")
        interactor.itemClicked(item)

        verify {
            savedLoginClicked.invoke(item)
        }
    }
}
