/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Intent
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.plus
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.android.FenixDialogFragment
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.PhoneFeature

/**
 * Dialog that presents the user with information about
 * - the current website and whether the connection is secured or not.
 * - website tracking protection.
 * - website permission.
 */
class QuickSettingsSheetDialogFragment : FenixDialogFragment() {

    private lateinit var quickSettingsStore: QuickSettingsFragmentStore
    private lateinit var quickSettingsController: QuickSettingsController
    private lateinit var websiteInfoView: WebsiteInfoView
    private lateinit var websitePermissionsView: WebsitePermissionsView
    private lateinit var trackingProtectionView: TrackingProtectionView
    private lateinit var interactor: QuickSettingsInteractor

    private var tryToRequestPermissions: Boolean = false
    private val args by navArgs<QuickSettingsSheetDialogFragmentArgs>()

    private lateinit var binding: FragmentQuickSettingsDialogSheetBinding

    override val gravity: Int get() = args.gravity
    override val layoutId: Int = R.layout.fragment_quick_settings_dialog_sheet

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val context = requireContext()
        val components = context.components

        val rootView = inflateRootView(container)
        binding = FragmentQuickSettingsDialogSheetBinding.bind(rootView)

        quickSettingsStore = QuickSettingsFragmentStore.createStore(
            context = context,
            websiteUrl = args.url,
            websiteTitle = args.title,
            isSecured = args.isSecured,
            permissions = args.sitePermissions,
            settings = components.settings,
            certificateName = args.certificateName,
            permissionHighlights = args.permissionHighlights,
            sessionId = args.sessionId,
            isTrackingProtectionEnabled = args.isTrackingProtectionEnabled
        )

        quickSettingsController = DefaultQuickSettingsController(
            context = context,
            quickSettingsStore = quickSettingsStore,
            browserStore = components.core.store,
            ioScope = viewLifecycleOwner.lifecycleScope + Dispatchers.IO,
            navController = findNavController(),
            sessionId = args.sessionId,
            sitePermissions = args.sitePermissions,
            settings = components.settings,
            permissionStorage = components.core.permissionStorage,
            reload = components.useCases.sessionUseCases.reload,
            addNewTab = components.useCases.tabsUseCases.addTab,
            requestRuntimePermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS)
                tryToRequestPermissions = true
            },
            displayPermissions = ::showPermissionsView,
            dismiss = ::dismiss
        )

        interactor = QuickSettingsInteractor(quickSettingsController)

        websiteInfoView = WebsiteInfoView(binding.websiteInfoLayout, interactor = interactor)
        websitePermissionsView =
            WebsitePermissionsView(binding.websitePermissionsLayout, interactor)
        trackingProtectionView =
            TrackingProtectionView(rootView.trackingProtectionLayout, interactor)

        return rootView
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(quickSettingsStore) {
            websiteInfoView.update(it.webInfoState)
            websitePermissionsView.update(it.websitePermissionsState)
            trackingProtectionView.update(it.trackingProtectionState)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        if (arePermissionsGranted(requestCode, grantResults)) {
            PhoneFeature.findFeatureBy(permissions)?.let {
                quickSettingsController.handleAndroidPermissionGranted(it)
            }
        } else {
            val shouldShowRequestPermissionRationale =
                permissions.all { shouldShowRequestPermissionRationale(it) }

            if (!shouldShowRequestPermissionRationale && tryToRequestPermissions) {
                // The user has permanently blocked these permissions and he/she is trying to enabling them.
                // at this point, we are not able to request these permissions, the only way to allow
                // them, it is to take the user to the system app setting page, and there the user
                // can allow the permissions.
                openSystemSettings()
            }
        }
        tryToRequestPermissions = false
    }

    private fun arePermissionsGranted(requestCode: Int, grantResults: IntArray) =
        requestCode == REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS && grantResults.all { it == PERMISSION_GRANTED }

    private fun showPermissionsView() {
        binding.websitePermissionsGroup.isVisible = true
    }

    private fun launchIntentReceiver() {
        context?.let { context ->
            val intent = Intent(context, IntentReceiverActivity::class.java)
            intent.action = Intent.ACTION_VIEW
            context.startActivity(intent)
        }
    }

    private fun openSystemSettings() {
        startActivity(
            Intent().apply {
                action = android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS
                data = Uri.fromParts("package", BuildConfig.APPLICATION_ID, null)
            }
        )
    }

    private companion object {
        const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4
    }
}
