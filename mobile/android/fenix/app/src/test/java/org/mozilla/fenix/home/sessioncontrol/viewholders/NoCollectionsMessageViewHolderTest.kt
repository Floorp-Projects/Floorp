/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.LayoutInflater
import androidx.core.view.isVisible
import androidx.lifecycle.LifecycleOwner
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.NoCollectionsMessageBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor

@RunWith(FenixRobolectricTestRunner::class)
class NoCollectionsMessageViewHolderTest {

    private lateinit var binding: NoCollectionsMessageBinding
    private val store: BrowserStore = BrowserStore(
        initialState = BrowserState(
            listOf(
                createTab("https://www.mozilla.org", id = "reader-inactive-tab")
            )
        )
    )
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var interactor: CollectionInteractor

    @Before
    fun setup() {
        binding = NoCollectionsMessageBinding.inflate(LayoutInflater.from(testContext))
        lifecycleOwner = mockk(relaxed = true)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `hide add to collection button when there are no tabs open`() {
        val noTabsStore = BrowserStore()
        NoCollectionsMessageViewHolder(binding.root, lifecycleOwner, noTabsStore, interactor)

        assertFalse(binding.addTabsToCollectionsButton.isVisible)
    }

    @Test
    fun `show add to collection button when there are tabs`() {
        NoCollectionsMessageViewHolder(binding.root, lifecycleOwner, store, interactor)

        assertTrue(binding.addTabsToCollectionsButton.isVisible)
    }

    @Test
    fun `call interactor on click`() {
        NoCollectionsMessageViewHolder(binding.root, lifecycleOwner, store, interactor)

        binding.addTabsToCollectionsButton.performClick()
        verify { interactor.onAddTabsToCollectionTapped() }
    }

    @Test
    fun `hide view and change setting on remove placeholder click`() {
        NoCollectionsMessageViewHolder(binding.root, lifecycleOwner, store, interactor)

        binding.removeCollectionPlaceholder.performClick()
        verify {
            interactor.onRemoveCollectionsPlaceholder()
        }
    }
}
