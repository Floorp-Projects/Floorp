/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.LayoutInflater
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.SearchSelectorBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SearchSelectorTest {

    private lateinit var searchSelector: SearchSelector
    private lateinit var binding: SearchSelectorBinding

    @Before
    fun setup() {
        searchSelector = SearchSelector(testContext)
        binding = SearchSelectorBinding.inflate(LayoutInflater.from(testContext), searchSelector)
    }

    @Test
    fun `WHEN set icon is called THEN an icon and its content description are set`() {
        val icon = AppCompatResources.getDrawable(testContext, R.drawable.ic_search)!!
        val contentDescription = "contentDescription"

        searchSelector.setIcon(icon, contentDescription)

        assertEquals(icon, binding.icon.drawable)
        assertEquals(contentDescription, binding.icon.contentDescription)
    }
}
