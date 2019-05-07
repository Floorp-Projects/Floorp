/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.app.Dialog
import android.content.Context
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity.BOTTOM
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.widget.NestedScrollView
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.exceptions.ExceptionDomains
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.settings.PhoneFeature
import java.net.MalformedURLException
import java.net.URL
import kotlin.coroutines.CoroutineContext

private const val KEY_URL = "KEY_URL"
private const val KEY_IS_SECURED = "KEY_IS_SECURED"
private const val KEY_SITE_PERMISSIONS = "KEY_SITE_PERMISSIONS"
private const val KEY_IS_TP_ON = "KEY_IS_TP_ON"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4

@SuppressWarnings("TooManyFunctions")
class QuickSettingsSheetDialogFragment : AppCompatDialogFragment(), CoroutineScope {
    private val safeArguments get() = requireNotNull(arguments)
    private val url: String by lazy { safeArguments.getString(KEY_URL) }
    private val isSecured: Boolean by lazy { safeArguments.getBoolean(KEY_IS_SECURED) }
    private val isTrackingProtectionOn: Boolean by lazy { safeArguments.getBoolean(KEY_IS_TP_ON) }
    private val promptGravity: Int by lazy { safeArguments.getInt(KEY_DIALOG_GRAVITY) }
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
    ): View {
        return inflateRootView(container)
    }

    private fun inflateRootView(container: ViewGroup? = null): View {
        val contextThemeWrapper = ContextThemeWrapper(
            activity,
            (activity as HomeActivity).themeManager.currentThemeResource
        )
        return LayoutInflater.from(contextThemeWrapper).inflate(
            R.layout.fragment_quick_settings_dialog_sheet,
            container,
            false
        )
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val customDialog = if (promptGravity == BOTTOM) {
            return BottomSheetDialog(requireContext(), this.theme)
        } else {
            Dialog(requireContext())
        }
        return customDialog.applyCustomizationsForTopDialog(inflateRootView())
    }

    private fun Dialog.applyCustomizationsForTopDialog(rootView: View): Dialog {
        addContentView(
            rootView,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        )

        window?.apply {
            setGravity(promptGravity)
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            // This must be called after addContentView, or it won't fully fill to the edge.
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        }
        return this
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        quickSettingsComponent = QuickSettingsComponent(
            rootView as NestedScrollView, ActionBusFactory.get(this),
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
            sitePermissions: SitePermissions?,
            gravity: Int = BOTTOM
        ): QuickSettingsSheetDialogFragment {

            val fragment = QuickSettingsSheetDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_URL, url)
                putBoolean(KEY_IS_SECURED, isSecured)
                putBoolean(KEY_IS_TP_ON, isTrackingProtectionOn)
                putParcelable(KEY_SITE_PERMISSIONS, sitePermissions)
                putInt(KEY_DIALOG_GRAVITY, gravity)
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

    private fun toggleTrackingProtection(context: Context, url: String) {
        val host = try {
            URL(url).host
        } catch (e: MalformedURLException) {
            url
        }
        launch {
            if (!ExceptionDomains.load(context).contains(host)) {
                ExceptionDomains.add(context, host)
            } else {
                ExceptionDomains.remove(context, listOf(host))
            }
        }
    }

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
                        dismiss()
                    }
                    is QuickSettingsAction.ToggleTrackingProtection -> {
                        val trackingEnabled = it.trackingProtection
                        context?.let { context: Context -> toggleTrackingProtection(context, url) }
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
