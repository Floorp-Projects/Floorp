/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.view.View
import android.widget.FrameLayout
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.MigrationRun
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.MigrationListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class MigrationStatusAdapterTest {

    private lateinit var adapter: MigrationStatusAdapter

    @Before
    fun setup() {
        adapter = MigrationStatusAdapter()
    }

    @Test
    fun `getItemCount should return the number of items in whitelist`() {
        assertEquals(0, adapter.itemCount)

        adapter.updateData(
            mapOf(
                Migration.Addons to MigrationRun(0, success = true),
                Migration.Settings to MigrationRun(0, success = true),
                Migration.Bookmarks to MigrationRun(0, success = false)
            )
        )
        assertEquals(4, adapter.itemCount)
    }

    @Test
    fun `creates and binds viewholder`() {
        adapter.updateData(
            mapOf(
                Migration.History to MigrationRun(0, success = true)
            )
        )

        val holder1 = adapter.createViewHolder(FrameLayout(testContext), 0)
        val holder2 = adapter.createViewHolder(FrameLayout(testContext), 0)
        val binding1 = MigrationListItemBinding.bind(holder1.itemView)
        val binding2 = MigrationListItemBinding.bind(holder2.itemView)
        adapter.bindViewHolder(holder1, 0)
        adapter.bindViewHolder(holder2, 1)

        assertEquals("Settings", binding1.migrationItemName.text)
        assertEquals(View.INVISIBLE, binding1.migrationStatusImage.visibility)

        assertEquals("History", binding2.migrationItemName.text)
        assertEquals(View.VISIBLE, binding2.migrationStatusImage.visibility)
        assertEquals("Migration completed", binding2.migrationStatusImage.contentDescription)
    }
}
