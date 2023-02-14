/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.widget.ListView
import android.widget.TextView
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.createAddedTestFragmentInNavHostActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.shadows.ShadowAlertDialog

@RunWith(FenixRobolectricTestRunner::class)
class AboutLibrariesFragmentTest {
    private lateinit var fragment: AboutLibrariesFragment
    private lateinit var librariesListView: ListView

    @Before
    fun setup() {
        fragment = createAddedTestFragmentInNavHostActivity { AboutLibrariesFragment() }
        librariesListView = fragment.requireView().findViewById(R.id.about_libraries_listview)
    }

    @Test
    fun `fragment should display licenses`() {
        assertTrue(0 < librariesListView.count)
    }

    @Test
    fun `item click should open license dialog`() {
        val listViewShadow = shadowOf(librariesListView)
        listViewShadow.clickFirstItemContainingText("org.mozilla.geckoview:geckoview")

        val alertDialogShadow = ShadowAlertDialog.getLatestDialog()
        assertTrue(alertDialogShadow.isShowing)

        val alertDialogText = alertDialogShadow
            .findViewById<TextView>(android.R.id.message)
            .text
            .toString()
        assertTrue(alertDialogText.contains("MPL"))
    }
}
