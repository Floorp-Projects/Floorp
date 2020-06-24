/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.LinearLayoutManager
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.share_close.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.share.listadapters.ShareTabsAdapter

@RunWith(FenixRobolectricTestRunner::class)
class ShareCloseViewTest {

    private lateinit var container: ViewGroup
    private lateinit var interactor: ShareCloseInteractor

    @Before
    fun setup() {
        container = FrameLayout(testContext)
        interactor = mockk(relaxUnitFun = true)
    }

    @Test
    fun `binds adapter and close button`() {
        ShareCloseView(container, interactor)

        assertTrue(container.shared_site_list.layoutManager is LinearLayoutManager)
        assertTrue(container.shared_site_list.adapter is ShareTabsAdapter)

        container.closeButton.performClick()
        verify { interactor.onShareClosed() }
    }
}
