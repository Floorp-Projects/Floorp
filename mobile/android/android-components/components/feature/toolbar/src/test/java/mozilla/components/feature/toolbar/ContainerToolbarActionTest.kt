/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ContainerToolbarActionTest {

    // Test container
    private val container = ContainerState(
        contextId = "contextId",
        name = "Personal",
        color = ContainerState.Color.GREEN,
        icon = ContainerState.Icon.CART,
    )

    @Test
    fun bind() {
        val imageView: ImageView = spy(ImageView(testContext))
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.container_action_image)).thenReturn(imageView)
        whenever(view.context).thenReturn(testContext)

        val action = spy(ContainerToolbarAction(container))
        action.bind(view)

        verify(imageView).contentDescription = container.name
        verify(imageView).setImageDrawable(any())
    }

    @Test
    fun createView() {
        var listenerWasClicked = false

        val action = ContainerToolbarAction(container, padding = Padding(1, 2, 3, 4)) {
            listenerWasClicked = true
        }

        val rootView = action.createView(LinearLayout(testContext))
        rootView.performClick()

        assertTrue(listenerWasClicked)
        assertEquals(rootView.paddingLeft, 1)
        assertEquals(rootView.paddingTop, 2)
        assertEquals(rootView.paddingRight, 3)
        assertEquals(rootView.paddingBottom, 4)
    }
}
