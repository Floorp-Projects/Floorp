/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.contextmenu

import android.content.Context
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt

class ContextMenuCandidatesTest {
    private val testContext: Context = mock()

    @Before
    fun setUp() {
        whenever(testContext.getString(anyInt())).thenReturn("dummy label")
    }

    @Test
    fun `WHEN the tab is a custom tab THEN the proper context candidates are created `() {
        // the expected list is the same as in a Fenix custom tab.
        val expectedCandidatesIDs = listOf(
            "mozac.feature.contextmenu.copy_link",
            "mozac.feature.contextmenu.share_link",
            "mozac.feature.contextmenu.save_image",
            "mozac.feature.contextmenu.save_video",
            "mozac.feature.contextmenu.copy_image_location",
        )

        val actualListIDs = ContextMenuCandidates.get(
            testContext,
            mock(),
            mock(),
            mock(),
            mock(),
            mock(),
            true,
        ).map {
            it.id
        }

        assertEquals(expectedCandidatesIDs, actualListIDs)
    }

    @Test
    fun `WHEN the tab is NOT a custom tab THEN the proper context candidates are created `() {
        val expectedCandidatesIDs = listOf(
            "mozac.feature.contextmenu.open_in_private_tab",
            "mozac.feature.contextmenu.copy_link",
            "mozac.feature.contextmenu.download_link",
            "mozac.feature.contextmenu.share_link",
            "mozac.feature.contextmenu.share_image",
            "mozac.feature.contextmenu.open_image_in_new_tab",
            "mozac.feature.contextmenu.save_image",
            "mozac.feature.contextmenu.save_video",
            "mozac.feature.contextmenu.copy_image_location",
            "mozac.feature.contextmenu.add_to_contact",
            "mozac.feature.contextmenu.share_email",
            "mozac.feature.contextmenu.copy_email_address",
            "mozac.feature.contextmenu.open_in_external_app",
        )

        val actualListIDs = ContextMenuCandidates.get(
            testContext,
            mock(),
            mock(),
            mock(),
            mock(),
            mock(),
            false,
        ).map {
            it.id
        }

        assertEquals(expectedCandidatesIDs, actualListIDs)
    }
}
