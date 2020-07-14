/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.LayoutInflater
import android.view.View
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.view.isVisible
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.no_collections_message.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor

@RunWith(FenixRobolectricTestRunner::class)
class NoCollectionsMessageViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: CollectionInteractor

    @Before
    fun setup() {
        val appCompatContext = ContextThemeWrapper(testContext, R.style.NormalTheme)
        view = LayoutInflater.from(appCompatContext)
            .inflate(NoCollectionsMessageViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `hide button when hasNormalTabsOpened is false`() {
        NoCollectionsMessageViewHolder(view, interactor, hasNormalTabsOpened = false)

        assertFalse(view.add_tabs_to_collections_button.isVisible)
    }

    @Test
    fun `show button when hasNormalTabsOpened is true`() {
        NoCollectionsMessageViewHolder(view, interactor, hasNormalTabsOpened = true)

        assertTrue(view.add_tabs_to_collections_button.isVisible)
    }

    @Test
    fun `call interactor on click`() {
        NoCollectionsMessageViewHolder(view, interactor, hasNormalTabsOpened = true)

        view.add_tabs_to_collections_button.performClick()
        verify { interactor.onAddTabsToCollectionTapped() }
    }
}
