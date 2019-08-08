/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity.BOTTOM
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.net.toUri
import androidx.core.widget.NestedScrollView
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.NavHostFragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.currentThemeResource
import org.mozilla.fenix.exceptions.ExceptionDomains
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.settings.PhoneFeature
import java.net.MalformedURLException
import java.net.URL
import com.google.android.material.R as MaterialR

private const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4

@ObsoleteCoroutinesApi
@SuppressWarnings("TooManyFunctions")
class QuickSettingsSheetDialogFragment : AppCompatDialogFragment() {
    private val safeArguments get() = requireNotNull(arguments)
    private val sessionId: String by lazy { QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).sessionId }
    private val url: String by lazy { QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).url }
    private val isSecured: Boolean by lazy { QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).isSecured }
    private val isTrackingProtectionOn: Boolean by lazy {
        QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).isTrackingProtectionOn
    }
    private val promptGravity: Int by lazy { QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).gravity }
    private lateinit var quickSettingsComponent: QuickSettingsComponent

    private var sitePermissions: SitePermissions? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        sitePermissions = QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).sitePermissions
        val rootView = inflateRootView(container)
        requireComponents.core.sessionManager.findSessionById(sessionId)?.register(sessionObserver, view = rootView)
        quickSettingsComponent = QuickSettingsComponent(
            rootView as NestedScrollView,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                QuickSettingsViewModel::class.java
            ) {
                QuickSettingsViewModel(
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
        )
        return rootView
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
        return if (promptGravity == BOTTOM) {
            BottomSheetDialog(requireContext(), this.theme).apply {
                setOnShowListener {
                    val bottomSheet =
                        findViewById<View>(MaterialR.id.design_bottom_sheet) as? FrameLayout
                    val behavior = BottomSheetBehavior.from(bottomSheet)
                    behavior.state = BottomSheetBehavior.STATE_EXPANDED
                }
            }
        } else {
            Dialog(requireContext()).applyCustomizationsForTopDialog(inflateRootView())
        }
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

    private fun arePermissionsGranted(requestCode: Int, grantResults: IntArray) =
        requestCode == REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS && grantResults.all { it == PERMISSION_GRANTED }

    private fun toggleTrackingProtection(context: Context, url: String) {
        val host = try {
            URL(url).host
        } catch (e: MalformedURLException) {
            url
        }
        lifecycleScope.launch {
            if (!ExceptionDomains.load(context).contains(host)) {
                ExceptionDomains.add(context, host)
            } else {
                ExceptionDomains.remove(context, listOf(host))
            }
        }
    }

    @ExperimentalCoroutinesApi
    @ObsoleteCoroutinesApi
    override fun onResume() {
        super.onResume()
        getAutoDisposeObservable<QuickSettingsAction>()
            .subscribe {
                when (it) {
                    is QuickSettingsAction.SelectBlockedByAndroid -> {
                        requestPermissions(it.permissions, REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS)
                    }
                    is QuickSettingsAction.SelectTrackingProtectionSettings -> {
                        val directions =
                            QuickSettingsSheetDialogFragmentDirections
                                .actionQuickSettingsSheetDialogFragmentToTrackingProtectionFragment()
                        findNavController(this@QuickSettingsSheetDialogFragment).navigate(directions)
                    }
                    is QuickSettingsAction.SelectReportProblem -> {
                        lifecycleScope.launch(Dispatchers.Main) {
                            val reportUrl =
                                String.format(BrowserFragment.REPORT_SITE_ISSUE_URL, it.url)
                            requireComponents.useCases.tabsUseCases.addTab.invoke(reportUrl)
                            val sessionManager = requireComponents.core.sessionManager
                            if (sessionManager.findSessionById(sessionId)?.isCustomTabSession() == true) {
                                val intent = Intent(context, IntentReceiverActivity::class.java)
                                intent.action = Intent.ACTION_VIEW
                                startActivity(intent)
                            }
                        }
                        dismiss()
                    }
                    is QuickSettingsAction.ToggleTrackingProtection -> {
                        val trackingEnabled = it.trackingProtection
                        context?.let { context: Context -> toggleTrackingProtection(context, url) }
                        lifecycleScope.launch(Dispatchers.Main) {
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

                        lifecycleScope.launch(Dispatchers.IO) {
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

    private val sessionObserver = object : Session.Observer {
        override fun onUrlChanged(session: Session, url: String) {
            lifecycleScope.launch(Dispatchers.IO) {
                val host = session.url.toUri().host
                val sitePermissions: SitePermissions? = host?.let {
                    val storage = requireContext().components.core.permissionStorage
                    storage.findSitePermissionsBy(it)
                }
                launch(Dispatchers.Main) {
                    getManagedEmitter<QuickSettingsChange>().onNext(
                        QuickSettingsChange.Change(
                            url,
                            session.securityInfo.secure,
                            session.trackerBlockingEnabled,
                            sitePermissions
                        )
                    )
                }
            }
        }

        override fun onTrackerBlockingEnabledChanged(session: Session, blockingEnabled: Boolean) {
            getManagedEmitter<QuickSettingsChange>().onNext(
                QuickSettingsChange.Change(
                    session.url,
                    session.securityInfo.secure,
                    blockingEnabled,
                    sitePermissions
                )
            )
        }

        override fun onSecurityChanged(session: Session, securityInfo: Session.SecurityInfo) {
            getManagedEmitter<QuickSettingsChange>().onNext(
                QuickSettingsChange.Change(
                    session.url,
                    securityInfo.secure,
                    session.trackerBlockingEnabled,
                    sitePermissions
                )
            )
        }
    }
}
