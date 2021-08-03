/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.LayoutInflater
import android.view.View
import androidx.navigation.Navigation
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.recent_bookmarks_header.view.*
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentBookmarksViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: SessionControlInteractor

    private val bookmark = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = null,
        url = null,
        dateAdded = 0,
        children = null
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(RecentBookmarksViewHolder.LAYOUT_ID, null)
        Navigation.setViewNavController(view, mockk(relaxed = true))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `WHEN show all bookmarks button is clicked THEN interactor is called`() {
        RecentBookmarksViewHolder(view, interactor).bind(listOf(bookmark))
        view.showAllBookmarksButton.performClick()

        verify { interactor.onShowAllBookmarksClicked() }
    }
}
