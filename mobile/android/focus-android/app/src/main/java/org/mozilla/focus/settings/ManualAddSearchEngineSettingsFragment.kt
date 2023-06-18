/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import androidx.annotation.WorkerThread
import androidx.core.view.forEach
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Request.Redirect.FOLLOW
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.util.URLStringUtils
import org.mozilla.focus.GleanMetrics.SearchEngines
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.search.ManualAddSearchEnginePreference
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.ViewUtils
import java.io.IOException
import java.net.MalformedURLException
import java.net.URL
import java.util.concurrent.TimeUnit

@Suppress("TooManyFunctions")
class ManualAddSearchEngineSettingsFragment : BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.manual_add_search_engine)
    }

    private var scope: CoroutineScope? = null
    private var menuItemForActiveAsyncTask: MenuItem? = null
    private var job: Job? = null

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.action_option_add_search_engine))
    }

    override fun onPause() {
        super.onPause()
        setUiIsValidatingAsync(false, menuItemForActiveAsyncTask)
        menuItemForActiveAsyncTask = null
    }

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        menuInflater.inflate(R.menu.menu_search_engine_manual_add, menu)
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean {
        val openLearnMore = {
            val learnMoreUrl = SupportUtils.getSumoURLForTopic(
                SupportUtils.getAppVersion(requireContext()),
                SupportUtils.SumoTopic.ADD_SEARCH_ENGINE,
            )
            SupportUtils.openUrlInCustomTab(requireActivity(), learnMoreUrl)
            SearchEngines.learnMoreTapped.record(NoExtras())

            true
        }

        val saveSearchEngine = {
            val engineName = requireView().findViewById<EditText>(R.id.edit_engine_name).text.toString()
            val searchQuery = requireView().findViewById<EditText>(R.id.edit_search_string).text.toString()

            val pref = findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)

            val existingEngines = requireContext().components.store.state.search.searchEngines
            val engineValid = pref?.validateEngineNameAndShowError(engineName, existingEngines) ?: false
            val searchValid = pref?.validateSearchQueryAndShowError(searchQuery) ?: false
            val isPartialSuccess = engineValid && searchValid

            if (isPartialSuccess) {
                ViewUtils.hideKeyboard(view)
                setUiIsValidatingAsync(true, menuItem)

                menuItemForActiveAsyncTask = menuItem
                scope?.launch {
                    validateSearchEngine(engineName, searchQuery, requireComponents.client)
                }
            } else {
                SearchEngines.saveEngineTapped.record(SearchEngines.SaveEngineTappedExtra(false))
            }

            true
        }

        return when (menuItem.itemId) {
            R.id.learn_more -> openLearnMore()
            R.id.menu_save_search_engine -> saveSearchEngine()
            else -> false
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        scope = CoroutineScope(Dispatchers.IO)
        return super.onCreateView(inflater, container, savedInstanceState)
    }

    override fun onDestroyView() {
        scope?.cancel()
        super.onDestroyView()
        if (view != null) ViewUtils.hideKeyboard(view)
    }

    private fun setUiIsValidatingAsync(isValidating: Boolean, saveMenuItem: MenuItem?) {
        val pref = findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)
        val updateViews = {
            // Disable text entry until done validating
            val viewGroup = view as ViewGroup
            enableAllSubviews(!isValidating, viewGroup)

            saveMenuItem?.isEnabled = !isValidating
        }

        if (isValidating) {
            view?.alpha = DISABLED_ALPHA
            // Delay showing the loading indicator to prevent it flashing on the screen
            job = scope?.launch(Dispatchers.Main) {
                delay(LOADING_INDICATOR_DELAY)
                pref?.setProgressViewShown(isValidating)
                updateViews()
            }
        } else {
            view?.alpha = 1f
            job?.cancel()
            pref?.setProgressViewShown(isValidating)
            updateViews()
        }
    }

    private fun enableAllSubviews(shouldEnable: Boolean, viewGroup: ViewGroup) {
        viewGroup.forEach { child ->
            if (child is ViewGroup) {
                enableAllSubviews(shouldEnable, child)
            } else {
                child.isEnabled = shouldEnable
            }
        }
    }

    private fun findManualAddSearchEnginePreference(id: Int): ManualAddSearchEnginePreference? {
        return findPreference(getString(id)) as? ManualAddSearchEnginePreference
    }

    companion object {
        private const val LOGTAG = "ManualAddSearchEngine"
        private const val SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS = 4000
        private const val VALID_RESPONSE_CODE_UPPER_BOUND = 300
        private const val DISABLED_ALPHA = 0.5f
        private const val LOADING_INDICATOR_DELAY: Long = 1000

        @WorkerThread
        @JvmStatic
        fun isValidSearchQueryURL(client: Client, query: String): Boolean {
            // we should share the code to substitute and normalize the search string (see SearchEngine.buildSearchUrl).
            val encodedTestQuery = Uri.encode("testSearchEngineValidation")

            val normalizedHttpsSearchURLStr = URLStringUtils.toNormalizedURL(query)
            val searchURLStr = normalizedHttpsSearchURLStr.replace("%s".toRegex(), encodedTestQuery)

            try { URL(searchURLStr) } catch (e: MalformedURLException) {
                // Don't log exception to avoid leaking URL.
                Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query")
                return false
            }

            val request = Request(
                url = searchURLStr,
                connectTimeout = SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS.toLong() to TimeUnit.MILLISECONDS,
                readTimeout = SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS.toLong() to TimeUnit.MILLISECONDS,
                redirect = FOLLOW,
                private = true,
            )

            return try {
                val response = client.fetch(request)
                // Close the response stream to ensure the body is closed correctly. See https://bugzilla.mozilla.org/show_bug.cgi?id=1603114.
                response.close()

                response.status < VALID_RESPONSE_CODE_UPPER_BOUND
            } catch (e: IOException) {
                Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query")
                false
            }
        }
    }

    private suspend fun validateSearchEngine(engineName: String, query: String, client: Client) {
        val isValidSearchQuery = isValidSearchQueryURL(client, query)

        withContext(Dispatchers.Main) {
            if (!isActive) {
                return@withContext
            }

            if (isValidSearchQuery) {
                requireComponents.searchUseCases.addSearchEngine(
                    createSearchEngine(
                        engineName,
                        query.toSearchUrl(),
                        requireComponents.icons.loadIcon(IconRequest(query, isPrivate = true)).await().bitmap,
                    ),
                )

                ViewUtils.showBrandedSnackbar(requireView(), R.string.search_add_confirmation, Snackbar.LENGTH_SHORT)
                requireActivity().settings.setDefaultSearchEngineByName(engineName)
                SearchEngines.saveEngineTapped.record(SearchEngines.SaveEngineTappedExtra(true))

                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(requireComponents.store.state.selectedTabId),
                )
            } else {
                showServerError()
                SearchEngines.saveEngineTapped.record(SearchEngines.SaveEngineTappedExtra(false))
            }

            setUiIsValidatingAsync(false, menuItemForActiveAsyncTask)
            menuItemForActiveAsyncTask = null
        }
    }

    private fun showServerError() {
        val pref = findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)
        pref?.setSearchQueryErrorText(getString(R.string.error_hostLookup_title))
    }
}

private fun String.toSearchUrl(): String {
    return replace("%s", "{searchTerms}")
}
