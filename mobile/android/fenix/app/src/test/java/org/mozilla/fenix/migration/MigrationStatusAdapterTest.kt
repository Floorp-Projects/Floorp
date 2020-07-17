/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.view.View
import android.widget.FrameLayout
import kotlinx.android.synthetic.main.migration_list_item.view.*
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.MigrationRun
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
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

        adapter.updateData(mapOf(
            Migration.Addons to MigrationRun(0, success = true),
            Migration.Settings to MigrationRun(0, success = true),
            Migration.Bookmarks to MigrationRun(0, success = false)
        ))
        assertEquals(4, adapter.itemCount)
    }

    @Test
    fun `creates and binds viewholder`() {
        adapter.updateData(mapOf(
            Migration.History to MigrationRun(0, success = true)
        ))

        val holder1 = adapter.createViewHolder(FrameLayout(testContext), 0)
        val holder2 = adapter.createViewHolder(FrameLayout(testContext), 0)
        adapter.bindViewHolder(holder1, 0)
        adapter.bindViewHolder(holder2, 1)

        assertEquals("Settings", holder1.itemView.migration_item_name.text)
        assertEquals(View.INVISIBLE, holder1.itemView.migration_status_image.visibility)

        assertEquals("History", holder2.itemView.migration_item_name.text)
        assertEquals(View.VISIBLE, holder2.itemView.migration_status_image.visibility)
        assertEquals("Migration completed", holder2.itemView.migration_status_image.contentDescription)
    }
}
