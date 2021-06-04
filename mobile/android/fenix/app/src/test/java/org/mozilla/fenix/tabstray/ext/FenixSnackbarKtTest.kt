package org.mozilla.fenix.tabstray.ext

import android.content.Context
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyOrder
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.tabstray.TabsTrayFragment.Companion.ELEVATION

class FenixSnackbarKtTest {

    @Test
    fun `WHEN message is called with different parameters THEN correct text will be set`() {
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

        snackbar.message(1, true)
        snackbar.message(2, false)
        snackbar.message(1, false)

        verifyOrder {
            snackbar.setText("test1")
            snackbar.setText("test2")
            snackbar.setText("test3")
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
        every { snackbar.setAction(any(), any()) }.answers { mockk(relaxed = true) }
        every { snackbar.anchorView }.answers { anchor }

        snackbar.anchorWithAction(anchor, mockk(relaxed = true))

        verifyOrder {
            snackbar.anchorView = anchor
            view.elevation = ELEVATION
            snackbar.setAction("test1", any())
        }
    }
}
