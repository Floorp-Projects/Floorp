/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.View
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class NimbusDetailItemViewHolderTest {

    private val branch = "hello"
    private lateinit var titleView: TextView
    private lateinit var summaryView: TextView

    @Before
    fun setup() {
        titleView = mock()
        summaryView = mock()
    }

    @Test
    fun `sets text on view`() {
        val view = View(testContext)
        val holder = NimbusDetailItemViewHolder(view, titleView, summaryView)

        holder.bind(branch)
        verify(titleView).text = branch
        verify(summaryView).text = branch
    }
}
