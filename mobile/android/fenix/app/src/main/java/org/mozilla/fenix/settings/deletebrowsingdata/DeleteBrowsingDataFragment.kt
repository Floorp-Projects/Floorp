/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import android.content.DialogInterface
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_delete_browsing_data.*
import kotlinx.android.synthetic.main.fragment_delete_browsing_data.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions", "LargeClass")
class DeleteBrowsingDataFragment : Fragment(R.layout.fragment_delete_browsing_data) {

    private lateinit var controller: DeleteBrowsingDataController
    private var scope: CoroutineScope? = null
    private lateinit var settings: Settings

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val tabsUseCases = requireComponents.useCases.tabsUseCases
        val downloadUseCases = requireComponents.useCases.downloadUseCases
        controller = DefaultDeleteBrowsingDataController(
            tabsUseCases.removeAllTabs,
            downloadUseCases.removeAllDownloads,
            requireComponents.core.historyStorage,
            requireComponents.core.permissionStorage,
            requireComponents.core.store,
            requireComponents.core.icons,
            requireComponents.core.engine
        )
        settings = requireContext().settings()

        getCheckboxes().iterator().forEach {
            it.onCheckListener = { _ ->
                updateDeleteButton()
                updatePreference(it)
            }
        }

        getCheckboxes().iterator().forEach {
            it.isChecked = when (it.id) {
                R.id.open_tabs_item -> settings.deleteOpenTabs
                R.id.browsing_data_item -> settings.deleteBrowsingHistory
                R.id.cookies_item -> settings.deleteCookies
                R.id.cached_files_item -> settings.deleteCache
                R.id.site_permissions_item -> settings.deleteSitePermissions
                R.id.downloads_item -> settings.deleteDownloads
                else -> true
            }
        }

        view.delete_data?.setOnClickListener {
            askToDelete()
        }
        updateDeleteButton()
    }

    private fun updatePreference(it: DeleteBrowsingDataItem) {
        when (it.id) {
            R.id.open_tabs_item -> settings.deleteOpenTabs = it.isChecked
            R.id.browsing_data_item -> settings.deleteBrowsingHistory = it.isChecked
            R.id.cookies_item -> settings.deleteCookies = it.isChecked
            R.id.cached_files_item -> settings.deleteCache = it.isChecked
            R.id.site_permissions_item -> settings.deleteSitePermissions = it.isChecked
            R.id.downloads_item -> settings.deleteDownloads = it.isChecked
            else -> return
        }
    }

    @ExperimentalCoroutinesApi
    override fun onStart() {
        super.onStart()

        scope = requireComponents.core.store.flowScoped(viewLifecycleOwner) { flow ->
            flow.map { state -> state.tabs.size }
                .ifChanged()
                .collect { openTabs -> updateTabCount(openTabs) }
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_delete_browsing_data))

        getCheckboxes().forEach {
            it.visibility = View.VISIBLE
        }

        updateItemCounts()
    }

    private fun updateDeleteButton() {
        val enabled = getCheckboxes().any { it.isChecked }

        view?.delete_data?.isEnabled = enabled
        view?.delete_data?.alpha = if (enabled) ENABLED_ALPHA else DISABLED_ALPHA
    }

    private fun askToDelete() {
        context?.let {
            AlertDialog.Builder(it).apply {
                setMessage(
                    it.getString(
                        R.string.delete_browsing_data_prompt_message_3,
                        it.getString(R.string.app_name)
                    )
                )

                setNegativeButton(R.string.delete_browsing_data_prompt_cancel) { it: DialogInterface, _ ->
                    it.cancel()
                }

                setPositiveButton(R.string.delete_browsing_data_prompt_allow) { it: DialogInterface, _ ->
                    it.dismiss()
                    deleteSelected()
                }
                create()
            }.show()
        }
    }

    private fun deleteSelected() {
        startDeletion()
        lifecycleScope.launch(IO) {
            getCheckboxes().mapIndexed { i, v ->
                if (v.isChecked) {
                    when (i) {
                        OPEN_TABS_INDEX -> controller.deleteTabs()
                        HISTORY_INDEX -> controller.deleteBrowsingData()
                        COOKIES_INDEX -> controller.deleteCookies()
                        CACHED_INDEX -> controller.deleteCachedFiles()
                        PERMS_INDEX -> controller.deleteSitePermissions()
                        DOWNLOADS_INDEX -> controller.deleteDownloads()
                    }
                }
            }

            withContext(Main) {
                finishDeletion()
                requireComponents.analytics.metrics.track(Event.ClearedPrivateData)
            }
        }
    }

    private fun startDeletion() {
        progress_bar.visibility = View.VISIBLE
        delete_browsing_data_wrapper.isEnabled = false
        delete_browsing_data_wrapper.isClickable = false
        delete_browsing_data_wrapper.alpha = DISABLED_ALPHA
    }

    private fun finishDeletion() {
        val popAfter = open_tabs_item.isChecked
        progress_bar.visibility = View.GONE
        delete_browsing_data_wrapper.isEnabled = true
        delete_browsing_data_wrapper.isClickable = true
        delete_browsing_data_wrapper.alpha = ENABLED_ALPHA

        updateItemCounts()

        FenixSnackbar.make(
            view = requireView(),
            duration = FenixSnackbar.LENGTH_SHORT,
            isDisplayedWithBrowserToolbar = true
        )
            .setText(resources.getString(R.string.preferences_delete_browsing_data_snackbar))
            .show()

        if (popAfter) viewLifecycleOwner.lifecycleScope.launch(
            Dispatchers.Main
        ) {

            findNavController().apply {
                // If the user deletes all open tabs we need to make sure we remove
                // the BrowserFragment from the backstack.
                popBackStack(R.id.homeFragment, false)
                navigate(DeleteBrowsingDataFragmentDirections.actionGlobalSettingsFragment())
            }
        }
    }

    override fun onPause() {
        super.onPause()
        progress_bar.visibility = View.GONE
    }

    override fun onStop() {
        super.onStop()
        scope?.cancel()
    }

    private fun updateItemCounts() {
        updateTabCount()
        updateHistoryCount()
        updateCookies()
        updateCachedImagesAndFiles()
        updateSitePermissions()
    }

    private fun updateTabCount(openTabs: Int = requireComponents.core.store.state.tabs.size) {
        view?.open_tabs_item?.apply {
            subtitleView.text = resources.getString(
                R.string.preferences_delete_browsing_data_tabs_subtitle,
                openTabs
            )
        }
    }

    private fun updateHistoryCount() {
        view?.browsing_data_item?.subtitleView?.text = ""

        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val historyCount = requireComponents.core.historyStorage.getVisited().size
            launch(Dispatchers.Main) {
                view?.browsing_data_item?.apply {
                    subtitleView.text =
                        resources.getString(
                            R.string.preferences_delete_browsing_data_browsing_data_subtitle,
                            historyCount
                        )
                }
            }
        }
    }

    private fun updateCookies() {
        // NO OP until we have GeckoView methods to count cookies
    }

    private fun updateCachedImagesAndFiles() {
        // NO OP until we have GeckoView methods to count cached images and files
    }

    private fun updateSitePermissions() {
        // NO OP until we have GeckoView methods for cookies and cached files, for consistency
    }

    private fun getCheckboxes(): List<DeleteBrowsingDataItem> {
        val fragmentView = requireView()
        return listOf(
            fragmentView.open_tabs_item,
            fragmentView.browsing_data_item,
            fragmentView.cookies_item,
            fragmentView.cached_files_item,
            fragmentView.site_permissions_item,
            fragmentView.downloads_item
        )
    }

    companion object {
        private const val ENABLED_ALPHA = 1f
        private const val DISABLED_ALPHA = 0.6f

        private const val OPEN_TABS_INDEX = 0
        private const val HISTORY_INDEX = 1
        private const val COOKIES_INDEX = 2
        private const val CACHED_INDEX = 3
        private const val PERMS_INDEX = 4
        private const val DOWNLOADS_INDEX = 5
    }
}
