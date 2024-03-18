/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.experiments.nimbus.Branch

@RunWith(AndroidJUnit4::class)
class NimbusBranchItemViewHolderTest {

    private val branch = Branch(
        slug = "control",
        ratio = 1,
    )
    private lateinit var nimbusBranchesDelegate: NimbusBranchesAdapterDelegate
    private lateinit var selectedIconView: ImageView
    private lateinit var titleView: TextView
    private lateinit var summaryView: TextView

    @Before
    fun setup() {
        nimbusBranchesDelegate = mock()
        selectedIconView = mock()
        titleView = mock()
        summaryView = mock()
    }

    @Test
    fun `GIVEN a branch WHEN bind is called THEN title and summary text is set`() {
        val view = View(testContext)
        val holder = NimbusBranchItemViewHolder(
            view,
            nimbusBranchesDelegate,
            selectedIconView,
            titleView,
            summaryView,
        )

        holder.bind(branch, "")

        verify(selectedIconView).isVisible = false
        verify(titleView).text = branch.slug
        verify(summaryView).text = branch.slug
    }

    @Test
    fun `WHEN item is clicked THEN delegate is called`() {
        val view = View(testContext)
        val holder =
            NimbusBranchItemViewHolder(
                view,
                nimbusBranchesDelegate,
                selectedIconView,
                titleView,
                summaryView,
            )

        holder.bind(branch, "")
        holder.itemView.performClick()

        verify(nimbusBranchesDelegate).onBranchItemClicked(branch)
    }

    @Test
    fun `WHEN the selected branch matches THEN the selected icon is visible`() {
        val view = View(testContext)
        val holder =
            NimbusBranchItemViewHolder(
                view,
                nimbusBranchesDelegate,
                selectedIconView,
                titleView,
                summaryView,
            )

        holder.bind(branch, branch.slug)

        verify(selectedIconView).isVisible = true
    }
}
