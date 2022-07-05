/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.CustomDomains
import org.mozilla.focus.GleanMetrics.Autocomplete
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentAutocompleteAddDomainBinding
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.ViewUtils
import kotlin.coroutines.CoroutineContext

/**
 * Fragment showing settings UI to add custom autocomplete domains.
 */
class AutocompleteAddFragment : BaseSettingsLikeFragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Main
    private var _binding: FragmentAutocompleteAddDomainBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        showToolbar(getString(R.string.preference_autocomplete_title_add))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentAutocompleteAddDomainBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        ViewUtils.showKeyboard(binding.domainView)
    }

    override fun onPause() {
        job.cancel()
        ViewUtils.hideKeyboard(activity?.currentFocus)
        super.onPause()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.menu_autocomplete_add, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.save) {

            val domain = binding.domainView.text.toString()
                .trim()
                .lowercase()

            launch(IO) {
                val domains = CustomDomains.load(requireActivity())
                val error = when {
                    domain.isEmpty() -> getString(R.string.preference_autocomplete_add_error)
                    domains.contains(domain) -> getString(R.string.preference_autocomplete_duplicate_url_error)
                    else -> null
                }

                launch(Main) {
                    if (error != null) {
                        binding.domainView.error = error
                    } else {
                        saveDomainAndClose(requireActivity().applicationContext, domain)
                    }
                }
            }

            return true
        }

        return super.onOptionsItemSelected(item)
    }

    private fun saveDomainAndClose(context: Context, domain: String) {
        launch(IO) {
            CustomDomains.add(context, domain)
            Autocomplete.domainAdded.add()
            TelemetryWrapper.saveAutocompleteDomainEvent(TelemetryWrapper.AutoCompleteEventSource.SETTINGS)
        }

        ViewUtils.showBrandedSnackbar(view, R.string.preference_autocomplete_add_confirmation, 0)

        requireComponents.appStore.dispatch(
            AppAction.NavigateUp(
                requireComponents.store.state.selectedTabId
            )
        )
    }
}
