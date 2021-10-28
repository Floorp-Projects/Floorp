/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.RadioButton
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.TabViewSettingChanged
import org.mozilla.fenix.components.metrics.Event.TabViewSettingChanged.Type
import org.mozilla.fenix.databinding.SurveyInactiveTabsDisableBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup
import java.util.Locale

/**
 * Lets the user customize auto closing tabs.
 */
@Suppress("TooManyFunctions")
class TabsSettingsFragment : PreferenceFragmentCompat() {
    private lateinit var listRadioButton: RadioButtonPreference
    private lateinit var gridRadioButton: RadioButtonPreference
    private lateinit var radioManual: RadioButtonPreference
    private lateinit var radioOneDay: RadioButtonPreference
    private lateinit var radioOneWeek: RadioButtonPreference
    private lateinit var radioOneMonth: RadioButtonPreference
    private lateinit var inactiveTabsCategory: PreferenceCategory
    private lateinit var inactiveTabs: SwitchPreference
    private lateinit var searchTermTabGroups: SwitchPreference
    private val shouldShowInactiveTabsTurnOffSurvey
        get() = requireContext().settings().isTelemetryEnabled &&
            requireContext().settings().shouldShowInactiveTabsTurnOffSurvey

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tabs_preferences, rootKey)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.context.components.analytics.metrics.track(Event.TabSettingsOpened)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_tabs))

        setupPreferences()
    }

    private fun setupPreferences() {
        // This should be the only use case for pref_key_tab_view_list_do_not_use.
        // In the Fenix impl of RadioGroups, we need to always have an individual pref value
        // for it to work. This is weird for a radio group that should hold a value from that group.
        // For the tabs tray, we only need a boolean value, so let's rely on only the
        // pref_key_tab_view_grid and look into using the native RadioGroup in the future.
        listRadioButton = requirePreference(R.string.pref_key_tab_view_list_do_not_use)
        gridRadioButton = requirePreference(R.string.pref_key_tab_view_grid)
        searchTermTabGroups = requirePreference<SwitchPreference>(R.string.pref_key_search_term_tab_groups).also {
            it.isVisible = FeatureFlags.tabGroupFeature
            it.isChecked = it.context.settings().searchTermTabGroupsAreEnabled
            it.onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        radioManual = requirePreference(R.string.pref_key_close_tabs_manually)
        radioOneMonth = requirePreference(R.string.pref_key_close_tabs_after_one_month)
        radioOneWeek = requirePreference(R.string.pref_key_close_tabs_after_one_week)
        radioOneDay = requirePreference(R.string.pref_key_close_tabs_after_one_day)

        inactiveTabs = requirePreference<SwitchPreference>(R.string.pref_key_inactive_tabs).also {
            it.isChecked = requireContext().settings().inactiveTabsAreEnabled
            it.setOnPreferenceChangeListener { preference, newValue ->
                if (shouldShowInactiveTabsTurnOffSurvey && newValue == false) {
                    // The first time the user tries to disable the feature show a little survey for her motives.
                    val inactiveTabsSurveyBinding = SurveyInactiveTabsDisableBinding.inflate(
                        LayoutInflater.from(context),
                        view as ViewGroup,
                        true
                    )
                    setupSurvey(inactiveTabsSurveyBinding)
                    requireContext().metrics.track(Event.InactiveTabsSurveyOpened)

                    // Don't update the preference as a direct action of user tapping the switch.
                    // Only disable the feature after the user selects an option in the survey or expressly closes it.
                    false
                } else {
                    SharedPreferenceUpdater().onPreferenceChange(preference, newValue)
                }
            }
        }

        inactiveTabsCategory = requirePreference<PreferenceCategory>(R.string.pref_key_inactive_tabs_category).also {
            it.isVisible = FeatureFlags.inactiveTabs
            it.isEnabled = !(it.context.settings().closeTabsAfterOneDay || it.context.settings().closeTabsAfterOneWeek)
        }

        listRadioButton.onClickListener(::sendTabViewTelemetry)
        gridRadioButton.onClickListener(::sendTabViewTelemetry)

        radioManual.onClickListener(::enableInactiveTabsSetting)
        radioOneDay.onClickListener(::disableInactiveTabsSetting)
        radioOneWeek.onClickListener(::disableInactiveTabsSetting)
        radioOneMonth.onClickListener(::enableInactiveTabsSetting)

        setupRadioGroups()
    }

    private fun setupSurvey(inactiveTabsSurveyBinding: SurveyInactiveTabsDisableBinding) {
        inactiveTabsSurveyBinding.closeSurvey.setOnClickListener {
            finishInactiveTabsSurvey(inactiveTabsSurveyBinding)

            // Register that user closed this survey without picking any option.
            requireContext().metrics.track(
                Event.InactiveTabsOffSurvey("none")
            )
        }

        // A map is needed to help retrieve the correct string on SEND.
        // These values are also sent to Glean which will truncate anything over 100 UTF8 characters.
        val radioButtonsMap: Map<Int, Int> = mapOf(
            R.id.rb_do_not_understand to R.string.inactive_tabs_survey_do_not_understand,
            R.id.rb_do_it_myself to R.string.inactive_tabs_survey_do_it_myself,
            R.id.rb_time_too_long to R.string.inactive_tabs_survey_time_too_long_option_1,
            R.id.rb_time_too_short to R.string.inactive_tabs_survey_time_too_short_option_1,
        )

        // Sets the Radio buttons' text
        radioButtonsMap.forEach {
            inactiveTabsSurveyBinding.surveyGroup.findViewById<RadioButton>(it.key)?.text =
                requireContext().getText(it.value)
        }

        inactiveTabsSurveyBinding.sendButton.setOnClickListener {
            val checkedRadioButtonId = inactiveTabsSurveyBinding.surveyGroup.checkedRadioButtonId
            // If no option has been selected the button does not need to do anything.
            if (checkedRadioButtonId != -1) {
                finishInactiveTabsSurvey(inactiveTabsSurveyBinding)

                // Using the stringId of the selected option an event is sent using English.
                radioButtonsMap[checkedRadioButtonId]?.let { stringId ->
                    requireContext().metrics.track(
                        Event.InactiveTabsOffSurvey(getDefaultString(stringId))
                    )
                }
            }
        }
    }

    /**
     * Set the inactive tabs survey completed and the feature disabled.
     */
    private fun finishInactiveTabsSurvey(inactiveTabsSurveyBinding: SurveyInactiveTabsDisableBinding) {
        inactiveTabsSurveyBinding.surveyContainer.visibility = View.GONE
        requireContext().settings().shouldShowInactiveTabsTurnOffSurvey = false
        requireContext().settings().inactiveTabsAreEnabled = false
        requirePreference<SwitchPreference>(R.string.pref_key_inactive_tabs).isChecked = false
    }

    /**
     * Get the "en-US" string value for the indicated [resourceId].
     */
    private fun getDefaultString(resourceId: Int): String {
        val config = Configuration(requireContext().resources.configuration)
        config.setLocale(Locale.ENGLISH)
        return requireContext().createConfigurationContext(config).getText(resourceId).toString()
    }

    private fun setupRadioGroups() {
        addToRadioGroup(
            listRadioButton,
            gridRadioButton
        )

        addToRadioGroup(
            radioManual,
            radioOneDay,
            radioOneMonth,
            radioOneWeek
        )
    }

    private fun sendTabViewTelemetry() {
        val metrics = requireContext().components.analytics.metrics

        if (listRadioButton.isChecked && !gridRadioButton.isChecked) {
            metrics.track(TabViewSettingChanged(Type.LIST))
        } else {
            metrics.track(TabViewSettingChanged(Type.GRID))
        }
    }

    private fun enableInactiveTabsSetting() {
        inactiveTabsCategory.apply {
            isEnabled = true
        }
    }

    private fun disableInactiveTabsSetting() {
        inactiveTabsCategory.apply {
            isEnabled = false
            inactiveTabs.isChecked = false
            context.settings().inactiveTabsAreEnabled = false
        }
    }
}
