/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.support.ktx.android.graphics.toDataUri
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.R
import org.mozilla.fenix.R.string.pref_key_leakcanary
import org.mozilla.fenix.R.string.pref_key_make_default_browser
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import java.io.File
import kotlin.coroutines.CoroutineContext

class SettingsFragment : PreferenceFragmentCompat(), CoroutineScope {

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()

        generateWordmark()
        setupPreferences()
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(R.string.pref_key_help) -> {
                requireComponents.useCases.tabsUseCases.addTab
                    .invoke(SupportUtils.getSumoURLForTopic(context!!, SupportUtils.SumoTopic.HELP))
                navigateToSettingsArticle()
            }
            resources.getString(R.string.pref_key_rate) -> {
                startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(SupportUtils.RATE_APP_URL)))
            }
            resources.getString(R.string.pref_key_feedback) -> {
                requireComponents.useCases.tabsUseCases.addTab.invoke(SupportUtils.FEEDBACK_URL)
                navigateToSettingsArticle()
            }
            resources.getString(R.string.pref_key_about) -> {
                requireComponents.useCases.tabsUseCases.addTab.invoke(aboutURL, true)
                navigateToSettingsArticle()
            }
        }
        return super.onPreferenceTreeClick(preference)
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    /**
     * Shrinks the wordmark resolution on first run to ensure About Page loads quickly
     */
    private fun generateWordmark() {
        val path = context?.filesDir
        val file = File(path, wordmarkPath)
        path?.let {
            if (!file.exists()) {
                launch(IO) {
                    val options = BitmapFactory.Options().apply {
                        inSampleSize = wordmarkScalingFactor
                        inJustDecodeBounds = false
                    }
                    file.appendText(
                        BitmapFactory.decodeResource(
                            resources,
                            R.drawable.ic_logo_wordmark, options
                        ).toDataUri()
                    )
                }
            }
        }
    }

    private fun setupPreferences() {
        val makeDefaultBrowserKey = context?.getPreferenceKey(pref_key_make_default_browser)
        val leakKey = context?.getPreferenceKey(pref_key_leakcanary)

        val preferenceMakeDefaultBrowser = findPreference<Preference>(makeDefaultBrowserKey)
        val preferenceLeakCanary = findPreference<Preference>(leakKey)

        preferenceMakeDefaultBrowser.onPreferenceClickListener =
            getClickListenerForMakeDefaultBrowser()

        preferenceLeakCanary.isVisible = BuildConfig.DEBUG
        if (BuildConfig.DEBUG) {
            preferenceLeakCanary.onPreferenceChangeListener =
                Preference.OnPreferenceChangeListener { _, newValue ->
                    (context?.applicationContext as FenixApplication).toggleLeakCanary(newValue as Boolean)
                    true
                }
        }
    }

    private val defaultClickListener = OnPreferenceClickListener { preference ->
        Toast.makeText(context, "${preference.title} Clicked", Toast.LENGTH_SHORT).show()
        true
    }

    private fun getClickListenerForMakeDefaultBrowser(): OnPreferenceClickListener {
        return if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
            OnPreferenceClickListener {
                val intent = Intent(
                    Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS
                )
                startActivity(intent)
                true
            }
        } else {
            defaultClickListener
        }
    }

    private fun navigateToSettingsArticle() {
        requireComponents.useCases.tabsUseCases.addTab.invoke(aboutURL, true)
        view?.let {
            Navigation.findNavController(it)
                .navigate(SettingsFragmentDirections.actionGlobalBrowser(null))
        }
    }

    companion object {
        const val wordmarkScalingFactor = 2
        const val wordmarkPath = "wordmark.b64"
        const val aboutURL = "about:version"
    }
}
