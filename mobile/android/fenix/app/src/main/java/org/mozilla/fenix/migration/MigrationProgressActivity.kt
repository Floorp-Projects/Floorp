/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.activity_migration.*
import kotlinx.android.synthetic.main.migration_list_item.view.*
import mozilla.components.support.migration.AbstractMigrationProgressActivity
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.Migration.Bookmarks
import mozilla.components.support.migration.Migration.History
import mozilla.components.support.migration.Migration.Logins
import mozilla.components.support.migration.Migration.Settings
import mozilla.components.support.migration.MigrationResults
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class MigrationProgressActivity : AbstractMigrationProgressActivity() {
    private val statusAdapter = MigrationStatusAdapter()
    override val store: MigrationStore by lazy { components.migrationStore }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_migration)
        init()
    }

    fun init() {
        migration_status_list.apply {
            layoutManager = LinearLayoutManager(this@MigrationProgressActivity)
            adapter = statusAdapter
        }

        migration_button.apply {
            setOnClickListener {
                finish()
                overridePendingTransition(0, 0)

                // If we received a user-initiated intent, throw this back to the intent receiver.
                if (intent.hasExtra(HomeActivity.OPEN_TO_BROWSER)) {
                    intent.setClassName(applicationContext, IntentReceiverActivity::class.java.name)
                    startActivity(intent)
                }
            }
            text = getString(
                R.string.migration_updating_app_button_text,
                getString(R.string.app_name)
            )
        }
    }

    override fun onMigrationCompleted() {
        // Enable clicking the finish button
        migration_button.apply {
            isEnabled = true
            text = getString(R.string.migration_update_app_button, getString(R.string.app_name))
            setBackgroundColor(ContextCompat.getColor(context, R.color.button_text_color))
            setTextColor(ContextCompat.getColor(context, R.color.white_color))
        }
    }

    override fun onMigrationStateChanged(progress: MigrationProgress, results: MigrationResults) {
        statusAdapter.submitList(results.toItemList())
    }
}

// These are the only items we want to show migrating in the UI.
internal val whiteList = mapOf(
    Bookmarks to R.string.preferences_sync_bookmarks,
    History to R.string.preferences_sync_history,
    Logins to R.string.preferences_sync_logins,
    Settings to R.string.settings_title
)

internal fun MigrationResults.toItemList() = filterKeys {
    whiteList.keys.contains(it)
}.map { (type, status) ->
    MigrationItem(
        type,
        status.success
    )
}

internal data class MigrationItem(val migration: Migration, val status: Boolean)

internal class MigrationStatusAdapter :
    ListAdapter<MigrationItem, MigrationStatusAdapter.ViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    override fun getItemViewType(position: Int): Int = R.layout.migration_list_item

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        private val context = view.context
        private val title = view.migration_item_name
        private val status = view.migration_status_image

        fun bind(item: MigrationItem) {
            // Get the resource ID for the item.
            val migrationText = whiteList[item.migration]?.run {
                context.getString(this)
            } ?: ""
            title.text = migrationText
            status.visibility = if (item.status) View.VISIBLE else View.INVISIBLE
            status.contentDescription =
                context.getString(R.string.migration_icon_description, migrationText)
        }
    }

    private object DiffCallback : DiffUtil.ItemCallback<MigrationItem>() {

        override fun areItemsTheSame(oldItem: MigrationItem, newItem: MigrationItem) =
            oldItem.migration.javaClass.simpleName == newItem.migration.javaClass.simpleName

        override fun areContentsTheSame(oldItem: MigrationItem, newItem: MigrationItem) =
            oldItem.migration.javaClass.simpleName == newItem.migration.javaClass.simpleName &&
                oldItem.status == newItem.status
    }
}
