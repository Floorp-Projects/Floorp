/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.content.Intent
import android.graphics.Rect
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.DimenRes
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.activity_migration.*
import kotlinx.android.synthetic.main.migration_list_item.view.*
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.migration.AbstractMigrationProgressActivity
import mozilla.components.support.migration.AbstractMigrationService
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.Migration.Bookmarks
import mozilla.components.support.migration.Migration.History
import mozilla.components.support.migration.Migration.Logins
import mozilla.components.support.migration.Migration.Settings
import mozilla.components.support.migration.MigrationResults
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class MigrationProgressActivity : AbstractMigrationProgressActivity() {
    private val logger = Logger("MigrationProgressActivity")
    private val statusAdapter = MigrationStatusAdapter()
    override val store: MigrationStore by lazy { components.migrationStore }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_migration)
        init()
    }

    fun init() {
        window.navigationBarColor = getColorFromAttr(R.attr.foundation)

        val appName = migration_description.context.getString(R.string.app_name)

        migration_description.apply {
            text = context.getString(R.string.migration_description, appName)
        }

        migration_status_list.apply {
            val margin = resources.getDimensionPixelSize(R.dimen.migration_margin)
            addItemDecoration(MigrationStatusItemDecoration(margin))
            layoutManager = LinearLayoutManager(this@MigrationProgressActivity)
            adapter = statusAdapter
        }

        migration_welcome_title.apply {
            text = context.getString(R.string.migration_title, appName)
        }

        migration_button_text_view.text = getString(R.string.migration_updating_app_button_text, appName)
    }

    override fun onMigrationCompleted(results: MigrationResults) {
        // Enable clicking the finish button
        migration_button_text_view.apply {
            setOnClickListener {
                AbstractMigrationService.dismissNotification(context)

                finish()
                overridePendingTransition(0, 0)

                store.dispatch(MigrationAction.Clear)

                // If we received a user-initiated intent, throw this back to the intent receiver.
                if (intent.hasExtra(HomeActivity.OPEN_TO_BROWSER)) {
                    intent.setClassName(applicationContext, IntentReceiverActivity::class.java.name)
                    startActivity(intent)
                } else {
                    // Fallback: Just launch the browser
                    logger.warn("Intent does not contain OPEN_TO_BROWSER extra, launching HomeActivity")
                    startActivity(Intent(this@MigrationProgressActivity, HomeActivity::class.java))
                }
            }
            text = getString(R.string.migration_update_app_button, getString(R.string.app_name))
            setTextColor(ContextCompat.getColor(context, R.color.white_color))
        }
        migration_button.setBackgroundResource(R.drawable.migration_button_background)
        migration_button_progress_bar.visibility = View.INVISIBLE
        // Keep the results list up-to-date.
        statusAdapter.submitList(results.toItemList())
    }

    override fun onMigrationStateChanged(progress: MigrationProgress, results: MigrationResults) {
        statusAdapter.submitList(results.toItemList())
    }
}

// These are the only items we want to show migrating in the UI.
internal val whiteList = linkedMapOf(
    Settings to R.string.settings_title,
    History to R.string.preferences_sync_history,
    Bookmarks to R.string.preferences_sync_bookmarks,
    Logins to R.string.migration_text_passwords
)

internal fun MigrationResults.toItemList() = whiteList.keys
    .map {
        if (containsKey(it)) {
            MigrationItem(it, getValue(it).success)
        } else {
            MigrationItem(it)
        }
    }

internal data class MigrationItem(val migration: Migration, val status: Boolean = false)

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
            status.contentDescription = context.getString(R.string.migration_icon_description)
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

internal class MigrationStatusItemDecoration(
    @DimenRes private val spacing: Int
) : RecyclerView.ItemDecoration() {

    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        val position = parent.getChildViewHolder(view).adapterPosition
        val itemCount = state.itemCount

        outRect.left = spacing
        outRect.right = spacing
        outRect.top = spacing
        outRect.bottom = if (position == itemCount - 1) spacing else 0
    }
}
