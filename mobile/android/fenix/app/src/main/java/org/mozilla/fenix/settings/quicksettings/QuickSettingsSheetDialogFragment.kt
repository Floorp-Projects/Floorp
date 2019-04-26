/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.preference.PreferenceManager
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.settings.PhoneFeature
import kotlin.coroutines.CoroutineContext
import androidx.appcompat.view.ContextThemeWrapper
import org.mozilla.fenix.R

private const val KEY_URL = "KEY_URL"
private const val KEY_IS_SECURED = "KEY_IS_SECURED"
private const val KEY_SITE_PERMISSIONS = "KEY_SITE_PERMISSIONS"
private const val KEY_IS_TP_ON = "KEY_IS_TP_ON"
private const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4

@SuppressWarnings("TooManyFunctions")
class QuickSettingsSheetDialogFragment : BottomSheetDialogFragment(), CoroutineScope {
    private val safeArguments get() = requireNotNull(arguments)
    private val url: String by lazy { safeArguments.getString(KEY_URL) }
    private val isSecured: Boolean by lazy { safeArguments.getBoolean(KEY_IS_SECURED) }
    private val isTrackingProtectionOn: Boolean by lazy { safeArguments.getBoolean(KEY_IS_TP_ON) }
    private lateinit var quickSettingsComponent: QuickSettingsComponent
    private lateinit var job: Job

    var sitePermissions: SitePermissions?
        get() = safeArguments.getParcelable(KEY_SITE_PERMISSIONS)
        set(value) {
            safeArguments.putParcelable(KEY_SITE_PERMISSIONS, value)
        }

    override val coroutineContext: CoroutineContext get() = Dispatchers.IO + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val contextThemeWrapper = ContextThemeWrapper(
            activity,
            (activity as HomeActivity).themeManager.currentThemeResource
        )
        val localInflater = inflater.cloneInContext(contextThemeWrapper)
        return localInflater.inflate(
            R.layout.fragment_quick_settings_dialog_sheet,
            container,
            false
        )
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        quickSettingsComponent = QuickSettingsComponent(
            rootView as ConstraintLayout, ActionBusFactory.get(this),
            QuickSettingsState(
                QuickSettingsState.Mode.Normal(
                    url,
                    isSecured,
                    isTrackingProtectionOn,
                    sitePermissions
                )
            )
        )
    }

    companion object {
        const val FRAGMENT_TAG = "QUICK_SETTINGS_FRAGMENT_TAG"

        fun newInstance(
            url: String,
            isSecured: Boolean,
            isTrackingProtectionOn: Boolean,
            sitePermissions: SitePermissions?
        ): QuickSettingsSheetDialogFragment {

            val fragment = QuickSettingsSheetDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_URL, url)
                putBoolean(KEY_IS_SECURED, isSecured)
                putBoolean(KEY_IS_TP_ON, isTrackingProtectionOn)
                putParcelable(KEY_SITE_PERMISSIONS, sitePermissions)
            }
            fragment.arguments = arguments
            return fragment
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        if (arePermissionsGranted(requestCode, grantResults)) {
            val feature = requireNotNull(PhoneFeature.findFeatureBy(permissions))
            getManagedEmitter<QuickSettingsChange>()
                .onNext(QuickSettingsChange.PermissionGranted(feature, sitePermissions))
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    private fun arePermissionsGranted(requestCode: Int, grantResults: IntArray) =
        requestCode == REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS && grantResults.all { it == PERMISSION_GRANTED }

    override fun onResume() {
        super.onResume()
        getAutoDisposeObservable<QuickSettingsAction>()
            .subscribe {
                when (it) {
                    is QuickSettingsAction.SelectBlockedByAndroid -> {
                        requestPermissions(it.permissions, REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS)
                    }
                    is QuickSettingsAction.SelectReportProblem -> {
                        launch(Dispatchers.Main) {
                            val reportUrl =
                                String.format(BrowserFragment.REPORT_SITE_ISSUE_URL, it.url)
                            requireComponents.useCases.sessionUseCases.loadUrl.invoke(reportUrl)
                        }
                    }
                    is QuickSettingsAction.ToggleTrackingProtection -> {
                        val trackingEnabled = it.trackingProtection
                        with(requireComponents.core) {
                            val policy =
                                createTrackingProtectionPolicy(trackingEnabled)
                            PreferenceManager.getDefaultSharedPreferences(context).edit()
                                .putBoolean(
                                    context!!.getString(R.string.pref_key_tracking_protection),
                                    trackingEnabled
                                ).apply()
                            engine.settings.trackingProtectionPolicy = policy

                            with(sessionManager) {
                                sessions.forEach {
                                    getEngineSession(it)?.enableTrackingProtection(
                                        policy
                                    )
                                }
                            }
                        }
                        launch(Dispatchers.Main) {
                            getManagedEmitter<QuickSettingsChange>().onNext(
                                QuickSettingsChange.Change(
                                    url,
                                    isSecured,
                                    trackingEnabled,
                                    sitePermissions
                                )
                            )
                            requireContext().components.useCases.sessionUseCases.reload.invoke()
                        }
                    }
                    is QuickSettingsAction.TogglePermission -> {

                        launch {
                            sitePermissions = quickSettingsComponent.toggleSitePermission(
                                context = requireContext(),
                                featurePhone = it.featurePhone,
                                url = url,
                                sitePermissions = sitePermissions
                            )

                            launch(Dispatchers.Main) {
                                getManagedEmitter<QuickSettingsChange>()
                                    .onNext(
                                        QuickSettingsChange.Stored(
                                            it.featurePhone,
                                            sitePermissions
                                        )
                                    )

                                requireContext().components.useCases.sessionUseCases.reload.invoke()
                            }
                        }
                    }
                }
            }

        if (isVisible) {
            getManagedEmitter<QuickSettingsChange>()
                .onNext(QuickSettingsChange.PromptRestarted(sitePermissions))
        }
    }
}
