/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.migration

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.migration.AbstractMigrationProgressActivity
import mozilla.components.support.migration.AbstractMigrationService
import mozilla.components.support.migration.MigrationResults
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ActivityMigrationBinding
import org.mozilla.fenix.ext.components

class MigrationProgressActivity : AbstractMigrationProgressActivity() {
    private val logger = Logger("MigrationProgressActivity")
    private val statusAdapter = MigrationStatusAdapter()
    override val store: MigrationStore by lazy { components.migrationStore }

    private lateinit var binding: ActivityMigrationBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMigrationBinding.inflate(layoutInflater)
        setContentView(binding.root)

        init()
    }

    fun init() {
        window.navigationBarColor = getColorFromAttr(R.attr.foundation)

        val appName = binding.migrationDescription.context.getString(R.string.app_name)

        binding.migrationDescription.apply {
            text = context.getString(R.string.migration_description, appName)
        }

        binding.migrationStatusList.apply {
            val margin = resources.getDimensionPixelSize(R.dimen.migration_margin)
            addItemDecoration(MigrationStatusItemDecoration(margin))
            layoutManager = LinearLayoutManager(this@MigrationProgressActivity)
            adapter = statusAdapter
        }

        binding.migrationWelcomeTitle.apply {
            text = context.getString(R.string.migration_title, appName)
        }

        binding.migrationButtonTextView.text = getString(R.string.migration_updating_app_button_text, appName)
    }

    override fun onMigrationCompleted(results: MigrationResults) {
        // Enable clicking the finish button
        binding.migrationButton.apply {
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
        }
        binding.migrationButtonTextView.apply {
            text = getString(R.string.migration_update_app_button, getString(R.string.app_name))
            setTextColor(ContextCompat.getColor(context, R.color.white_color))
        }
        binding.migrationButton.setBackgroundResource(R.drawable.migration_button_background)
        binding.migrationButtonProgressBar.visibility = View.INVISIBLE
        // Keep the results list up-to-date.
        statusAdapter.updateData(results)
    }

    override fun onMigrationStateChanged(progress: MigrationProgress, results: MigrationResults) {
        statusAdapter.updateData(results)
    }
}
