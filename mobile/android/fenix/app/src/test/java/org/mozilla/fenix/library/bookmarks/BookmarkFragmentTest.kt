/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import androidx.fragment.app.testing.FragmentScenario
import androidx.fragment.app.testing.launchFragmentInContainer
import androidx.navigation.NavController
import androidx.navigation.Navigation
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import mozilla.appservices.places.BookmarkRoot
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.TestUtils
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class BookmarkFragmentTest {

    private lateinit var scenario: FragmentScenario<BookmarkFragment>

    @Before
    fun setup() {
        TestUtils.setRxSchedulers()

        val mockNavController = mockk<NavController>()
        every { mockNavController.addOnDestinationChangedListener(any()) } just Runs

        val args = BookmarkFragmentArgs(BookmarkRoot.Mobile.id).toBundle()
        scenario =
            launchFragmentInContainer<BookmarkFragment>(fragmentArgs = args, themeResId = R.style.NormalTheme) {
                BookmarkFragment().also { fragment ->
                    fragment.viewLifecycleOwnerLiveData.observeForever {
                        if (it != null) {
                            Navigation.setViewNavController(fragment.requireView(), mockNavController)
                        }
                    }
                }
            }
    }

    @Test
    fun `test initial bookmarks fragment ui`() {
        scenario.onFragment { fragment ->
            assertEquals(fragment.getString(R.string.library_bookmarks), fragment.activity?.title)
        }
    }
}
