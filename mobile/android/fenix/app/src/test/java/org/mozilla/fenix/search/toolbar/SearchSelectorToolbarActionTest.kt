/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.SearchDialogFragmentStore

@RunWith(FenixRobolectricTestRunner::class)
class SearchSelectorToolbarActionTest {

    @MockK(relaxed = true)
    private lateinit var store: SearchDialogFragmentStore

    @MockK(relaxed = true)
    private lateinit var menu: SearchSelectorMenu

    private lateinit var lifecycleOwner: MockedLifecycleOwner

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)
    }

    @Test
    fun `WHEN search selector toolbar action is clicked THEN the search selector menu is shown`() {
        val action = spyk(
            SearchSelectorToolbarAction(
                store = store,
                menu = menu,
                viewLifecycleOwner = lifecycleOwner
            )
        )
        val view = action.createView(LinearLayout(testContext) as ViewGroup) as SearchSelector

        view.performClick()

        verify {
            menu.menuController.show(view)
        }
    }
}
