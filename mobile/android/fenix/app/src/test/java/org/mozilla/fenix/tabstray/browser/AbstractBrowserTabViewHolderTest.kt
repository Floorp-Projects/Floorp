/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore

@RunWith(FenixRobolectricTestRunner::class)
class AbstractBrowserTabViewHolderTest {
    val store = TabsTrayStore()
    val browserStore = BrowserStore()
    val interactor = mockk<BrowserTrayInteractor>(relaxed = true)

    @Test
    fun `WHEN itemView is clicked THEN interactor invokes open`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.tab_tray_item, null)
        val holder = TestTabTrayViewHolder(
            view,
            mockk(relaxed = true),
            store,
            null,
            browserStore,
            mockk(relaxed = true),
            interactor
        )

        holder.bind(createTab(), false, mockk(), mockk())

        holder.itemView.performClick()

        verify { interactor.open(any()) }
    }

    @Test
    fun `WHEN itemView is clicked with a selection holder THEN the select holder is invoked`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.tab_tray_item, null)
        val selectionHolder = TestSelectionHolder(emptySet())
        val holder = TestTabTrayViewHolder(
            view,
            mockk(relaxed = true),
            store,
            selectionHolder,
            browserStore,
            mockk(relaxed = true),
            interactor
        )

        holder.bind(createTab(), false, mockk(), mockk())

        holder.itemView.performClick()

        verify { interactor.open(any()) }
        assertTrue(selectionHolder.invoked)
    }

    @Suppress("LongParameterList")
    class TestTabTrayViewHolder(
        itemView: View,
        imageLoader: ImageLoader,
        trayStore: TabsTrayStore,
        selectionHolder: SelectionHolder<Tab>?,
        store: BrowserStore,
        metrics: MetricController,
        override val browserTrayInteractor: BrowserTrayInteractor
    ) : AbstractBrowserTabViewHolder(itemView, imageLoader, trayStore, selectionHolder, store, metrics) {
        override val thumbnailSize: Int
            get() = 30

        override fun updateSelectedTabIndicator(showAsSelected: Boolean) {
            // do nothing
        }
    }

    class TestSelectionHolder(
        private val testItems: Set<Tab>
    ) : SelectionHolder<Tab> {
        override val selectedItems: Set<Tab>
            get() {
                invoked = true
                return testItems
            }

        var invoked = false
    }
}
