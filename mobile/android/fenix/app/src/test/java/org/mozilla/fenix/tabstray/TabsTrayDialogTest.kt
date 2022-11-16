/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor

class TabsTrayDialogTest {
    @Test
    fun `WHEN onBackPressed THEN invoke interactor`() {
        val context = mockk<Context>(relaxed = true)
        val interactor = mockk<BrowserTrayInteractor>(relaxed = true)
        val dialog = TabsTrayDialog(context, 0) { interactor }

        @Suppress("DEPRECATION")
        dialog.onBackPressed()

        verify { interactor.onBackPressed() }
    }
}
