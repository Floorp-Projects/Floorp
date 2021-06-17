/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.app.Dialog
import android.content.Intent
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Bundle
import android.view.Gravity.BOTTOM
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.*
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.plus
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.PhoneFeature
import com.google.android.material.R as MaterialR

/**
 * Dialog that presents the user with information about
 * - the current website and whether the connection is secured or not.
 * - website tracking protection.
 * - website permission.
 */
class QuickSettingsSheetDialogFragment : AppCompatDialogFragment() {

    private lateinit var quickSettingsStore: QuickSettingsFragmentStore
    private lateinit var quickSettingsController: QuickSettingsController
    private lateinit var websiteInfoView: WebsiteInfoView
    private lateinit var websitePermissionsView: WebsitePermissionsView
    private lateinit var interactor: QuickSettingsInteractor
    private var tryToRequestPermissions: Boolean = false
    private val args by navArgs<QuickSettingsSheetDialogFragmentArgs>()

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
        quickSettingsStore = QuickSettingsFragmentStore.createStore(
            context = context,
            websiteUrl = args.url,
            websiteTitle = args.title,
            isSecured = args.isSecured,
            permissions = args.sitePermissions,
            settings = components.settings,
            certificateName = args.certificateName,
            permissionHighlights = args.permissionHighlights
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

        websiteInfoView = WebsiteInfoView(rootView.websiteInfoLayout)
        websitePermissionsView =
            WebsitePermissionsView(rootView.websitePermissionsLayout, interactor)

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
        return if (args.gravity == BOTTOM) {
            BottomSheetDialog(requireContext(), this.theme).apply {
                setOnShowListener {
                    val bottomSheet =
                        findViewById<View>(MaterialR.id.design_bottom_sheet) as FrameLayout
                    val behavior = BottomSheetBehavior.from(bottomSheet)
                    behavior.state = BottomSheetBehavior.STATE_EXPANDED
                }
            }
        } else {
            Dialog(requireContext()).applyCustomizationsForTopDialog(inflateRootView())
        }
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(quickSettingsStore) {
            websiteInfoView.update(it.webInfoState)
            websitePermissionsView.update(it.websitePermissionsState)
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
            val shouldShowRequestPermissionRationale = permissions.all { shouldShowRequestPermissionRationale(it) }

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

    private fun Dialog.applyCustomizationsForTopDialog(rootView: View): Dialog {
        addContentView(
            rootView,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        )

        window?.apply {
            setGravity(args.gravity)
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            // This must be called after addContentView, or it won't fully fill to the edge.
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        }
        return this
    }

    private fun arePermissionsGranted(requestCode: Int, grantResults: IntArray) =
        requestCode == REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS && grantResults.all { it == PERMISSION_GRANTED }

    private fun showPermissionsView() {
        websitePermissionsGroup.isVisible = true
    }

    private fun launchIntentReceiver() {
        context?.let { context ->
            val intent = Intent(context, IntentReceiverActivity::class.java)
            intent.action = Intent.ACTION_VIEW
            context.startActivity(intent)
        }
    }

    private fun openSystemSettings() {
        startActivity(Intent().apply {
            action = android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS
            data = Uri.fromParts("package", BuildConfig.APPLICATION_ID, null)
        })
    }

    private companion object {
        const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4
    }
}
