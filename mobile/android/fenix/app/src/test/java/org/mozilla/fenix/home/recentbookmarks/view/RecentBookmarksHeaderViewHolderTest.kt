/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.LayoutInflater
import androidx.navigation.Navigation
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.RecentBookmarksHeaderBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentBookmarksHeaderViewHolderTest {

    private lateinit var binding: RecentBookmarksHeaderBinding
    private lateinit var interactor: RecentBookmarksInteractor

    @Before
    fun setup() {
        binding = RecentBookmarksHeaderBinding.inflate(LayoutInflater.from(testContext))
        Navigation.setViewNavController(binding.root, mockk(relaxed = true))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `WHEN show all button is clicked THEN interactor is called`() {
        RecentBookmarksHeaderViewHolder(binding.root, interactor)

        binding.showAllBookmarksButton.performClick()

        verify { interactor.onShowAllBookmarksClicked() }
    }
}
