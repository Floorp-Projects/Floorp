/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.net.Uri
import android.os.AsyncTask
import android.os.Bundle
import android.os.Handler
import android.util.Log
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.ViewGroup
import android.widget.EditText
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import com.google.android.material.snackbar.Snackbar
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.search.CustomSearchEngineStore
import org.mozilla.focus.search.ManualAddSearchEnginePreference
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import org.mozilla.focus.utils.createTab
import java.io.IOException
import java.lang.ref.WeakReference
import java.net.HttpURLConnection
import java.net.MalformedURLException
import java.net.URL

class ManualAddSearchEngineSettingsFragment : BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        setHasOptionsMenu(true)

        addPreferencesFromResource(R.xml.manual_add_search_engine)
    }

    /**
     * A reference to an active async task, if applicable, used to manage the task for lifecycle changes.
     * See {@link #onPause()} for details.
     */
    private var activeAsyncTask: AsyncTask<Void, Void, Boolean>? = null
    private var menuItemForActiveAsyncTask: MenuItem? = null
    private val handler = Handler()

    override fun onResume() {
        super.onResume()

        // We've checked that this cast is legal in super.onAttach.
        (activity as? ActionBarUpdater)?.updateIcon(R.drawable.ic_close)

        val updater = getActionBarUpdater()
        updater.updateTitle(R.string.action_option_add_search_engine)
        updater.updateIcon(R.drawable.ic_close)
    }

    override fun onPause() {
        super.onPause()
        // This is a last minute change and we want to keep the async task management simple: onPause is the
        // first required callback for various lifecycle changes: a dialog is shown, the user
        // leaves the app, the app rotates, etc. To keep things simple, we do our AsyncTask management here,
        // before it gets more complex (e.g. reattaching the AsyncTask to a new fragment).
        //
        // We cancel the AsyncTask also to keep things simple: if the task is cancelled, it will:
        // - Likely end immediately and we don't need to handle it returning after the lifecycle changes
        // - Get onPostExecute scheduled on the UI thread, which must run after onPause (since it also runs on
        // the UI thread), and we check if the AsyncTask is cancelled there before we perform any other actions.
        if (activeAsyncTask == null) return
        activeAsyncTask?.cancel(true)
        setUiIsValidatingAsync(false, menuItemForActiveAsyncTask)

        activeAsyncTask = null
        menuItemForActiveAsyncTask = null
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.menu_search_engine_manual_add, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        val openLearnMore = {
            val session = createTab(
                SupportUtils.getSumoURLForTopic(requireContext(), SupportUtils.SumoTopic.ADD_SEARCH_ENGINE)
            )

            components?.sessionManager?.add(session, selected = true)

            TelemetryWrapper.addSearchEngineLearnMoreEvent()

            activity?.finish()
        }

        val saveSearchEngine = {
            val engineName = view!!.findViewById<EditText>(R.id.edit_engine_name).text.toString()
            val searchQuery = view!!.findViewById<EditText>(R.id.edit_search_string).text.toString()

            val pref = findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)
            val engineValid = pref.validateEngineNameAndShowError(engineName)
            val searchValid = pref.validateSearchQueryAndShowError(searchQuery)
            val isPartialSuccess = engineValid && searchValid

            if (isPartialSuccess) {
                ViewUtils.hideKeyboard(view)
                setUiIsValidatingAsync(true, item)
                activeAsyncTask = ValidateSearchEngineAsyncTask(this, engineName, searchQuery).execute()
                menuItemForActiveAsyncTask = item
            } else {
                TelemetryWrapper.saveCustomSearchEngineEvent(false)
            }
        }

        when (item.itemId) {
            R.id.learn_more -> openLearnMore()
            R.id.menu_save_search_engine -> saveSearchEngine()
            else -> return super.onOptionsItemSelected(item)
        }

        return true
    }

    override fun onDestroyView() {
        super.onDestroyView()
        if (view != null) ViewUtils.hideKeyboard(view)
    }

    private fun setUiIsValidatingAsync(isValidating: Boolean, saveMenuItem: MenuItem?) {
        val pref = findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)

        if (isValidating) {
            view?.alpha = DISABLED_ALPHA
            // Delay showing the loading indicator to prevent it flashing on the screen
            handler.postDelayed({
                pref.setProgressViewShown(isValidating)
            }, LOADING_INDICATOR_DELAY)
        } else {
            view?.alpha = 1f
            handler.removeCallbacksAndMessages(null)
            pref.setProgressViewShown(isValidating)
        }

        // Disable text entry until done validating
        val viewGroup = view as ViewGroup
        enableAllSubviews(!isValidating, viewGroup)

        saveMenuItem!!.isEnabled = !isValidating
    }

    private fun enableAllSubviews(shouldEnable: Boolean, viewGroup: ViewGroup) {
        for (i in 0 until viewGroup.childCount) {
            val child = viewGroup.getChildAt(i)
            if (child is ViewGroup) {
                enableAllSubviews(shouldEnable, child)
            } else {
                child.isEnabled = shouldEnable
            }
        }
    }

    private fun findManualAddSearchEnginePreference(id: Int): ManualAddSearchEnginePreference {
        return findPreference(getString(id)) as ManualAddSearchEnginePreference
    }

    companion object {
        private val LOGTAG = "ManualAddSearchEngine"
        private val SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS = 4000
        private val VALID_RESPONSE_CODE_UPPER_BOUND = 300
        private const val DISABLED_ALPHA = 0.5f
        private const val LOADING_INDICATOR_DELAY: Long = 1000

        @SuppressWarnings("DE_MIGHT_IGNORE")
        @WorkerThread
        @JvmStatic
        @VisibleForTesting fun isValidSearchQueryURL(query: String): Boolean {
            // we should share the code to substitute and normalize the search string (see SearchEngine.buildSearchUrl).
            val encodedTestQuery = Uri.encode("testSearchEngineValidation")

            val normalizedHttpsSearchURLStr = UrlUtils.normalize(query)
            val searchURLStr = normalizedHttpsSearchURLStr.replace("%s".toRegex(), encodedTestQuery)
            val searchURL = try { URL(searchURLStr) } catch (e: MalformedURLException) {
                // Don't log exception to avoid leaking URL.
                Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query")
                return false
            }

            val connection = searchURL.openConnection() as HttpURLConnection
            connection.instanceFollowRedirects = true
            connection.connectTimeout = SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS
            connection.readTimeout = SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS

            return try {
                connection.responseCode < VALID_RESPONSE_CODE_UPPER_BOUND
            } catch (e: IOException) {
                Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query")
                false
            } finally {
                try { connection.inputStream.close() } catch (_: IOException) {
                    Log.d(LOGTAG, "connection.inputStream failed to close")
                }

                connection.disconnect()
            }
        }
    }

    private class ValidateSearchEngineAsyncTask
        constructor (
            fragment: ManualAddSearchEngineSettingsFragment,
            private val engineName: String,
            private val query: String
        ) : AsyncTask<Void, Void, Boolean>() {

        private val fragmentWeakReference = WeakReference(fragment)

        override fun doInBackground(vararg p0: Void?): Boolean {
            val isValidSearchQuery = isValidSearchQueryURL(query)
            TelemetryWrapper.saveCustomSearchEngineEvent(isValidSearchQuery)

            return isValidSearchQuery
        }

        override fun onPostExecute(isValidSearchQuery: Boolean) {
            super.onPostExecute(isValidSearchQuery)
            if (isCancelled) {
                Log.d(LOGTAG, "ValidateSearchEngineAsyncTask has been cancelled")
                return
            }

            val fragment = fragmentWeakReference.get()
            if (fragment == null) {
                Log.d(LOGTAG, "Fragment or menu item no longer exists when search query " +
                        "validation async task returned.")
                return
            }

            if (isValidSearchQuery) {
                CustomSearchEngineStore.addSearchEngine(fragment.requireActivity(), engineName, query)
                Snackbar.make(fragment.requireView(), R.string.search_add_confirmation, Snackbar.LENGTH_SHORT).show()
                Settings.getInstance(fragment.requireActivity()).setDefaultSearchEngineByName(engineName)
                @Suppress("DEPRECATION")
                fragment.requireFragmentManager().popBackStack()
            } else {
                showServerError(fragment)
            }

            fragment.setUiIsValidatingAsync(false, fragment.menuItemForActiveAsyncTask)
            fragment.activeAsyncTask = null
            fragment.menuItemForActiveAsyncTask = null
        }

        private fun showServerError(that: ManualAddSearchEngineSettingsFragment) {
            val pref = that.findManualAddSearchEnginePreference(R.string.pref_key_manual_add_search_engine)
            pref.setSearchQueryErrorText(that.getString(R.string.error_hostLookup_title))
        }
    }
}
