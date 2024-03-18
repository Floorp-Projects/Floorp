/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.content.Context
import android.view.View
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verifyOrder
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FenixSnackbarBehavior
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.tabstray.TabsTrayFragment.Companion.ELEVATION
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class FenixSnackbarKtTest {

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Test
    fun `WHEN collectionMessage is called with different parameters THEN correct text will be set`() {
        val mockContext: Context = mockk {
            every { getString(R.string.create_collection_tabs_saved_new_collection) }
                .answers { "test1" }
            every { getString(R.string.create_collection_tabs_saved) }
                .answers { "test2" }
            every { getString(R.string.create_collection_tab_saved) }
                .answers { "test3" }
        }
        val snackbar: FenixSnackbar = mockk {
            every { context }.answers { mockContext }
        }
        every { snackbar.setText(any()) }.answers { snackbar }

        snackbar.collectionMessage(1, true)
        snackbar.collectionMessage(2, false)
        snackbar.collectionMessage(1, false)

        verifyOrder {
            snackbar.setText("test1")
            snackbar.setText("test2")
            snackbar.setText("test3")
        }
    }

    @Test
    fun `WHEN bookmarkMessage is called with different parameters THEN correct text will be set`() {
        val mockContext: Context = mockk {
            every { getString(R.string.bookmark_saved_snackbar) }
                .answers { "test1" }
            every { getString(R.string.snackbar_message_bookmarks_saved) }
                .answers { "test2" }
        }
        val snackbar: FenixSnackbar = mockk {
            every { context }.answers { mockContext }
        }
        every { snackbar.setText(any()) }.answers { snackbar }

        snackbar.bookmarkMessage(1)
        snackbar.bookmarkMessage(2)

        verifyOrder {
            snackbar.setText("test1")
            snackbar.setText("test2")
        }
    }

    @Test
    fun `WHEN anchorWithAction is called THEN correct text will be set`() {
        val mockContext: Context = mockk {
            every { getString(R.string.create_collection_view) }
                .answers { "test1" }
        }
        val anchor: View = mockk(relaxed = true)
        val view: View = mockk(relaxed = true)
        val snackbar: FenixSnackbar = mockk {
            every { context }.answers { mockContext }
        }

        every { snackbar.setAnchorView(anchor) }.answers { snackbar }
        every { snackbar.view }.answers { view }
        every { snackbar.setAction(any(), any()) }.answers { snackbar }
        every { snackbar.anchorView }.answers { anchor }

        snackbar.anchorWithAction(anchor, {})

        verifyOrder {
            snackbar.anchorView = anchor
            view.elevation = ELEVATION
            snackbar.setAction("test1", any())
        }
    }

    @Test
    fun `GIVEN the snackbar is a child of dynamic container WHEN it is shown THEN enable the dynamic behavior`() {
        val container = FrameLayout(testContext).apply {
            id = R.id.dynamicSnackbarContainer
            layoutParams = CoordinatorLayout.LayoutParams(0, 0)
        }
        val settings: Settings = mockk(relaxed = true) {
            every { toolbarPosition } returns ToolbarPosition.BOTTOM
        }
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns settings

            FenixSnackbar.make(view = container)

            val behavior = (container.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior
            assertTrue(behavior is FenixSnackbarBehavior)
            assertEquals(ToolbarPosition.BOTTOM, (behavior as? FenixSnackbarBehavior)?.toolbarPosition)
        }
    }
}
