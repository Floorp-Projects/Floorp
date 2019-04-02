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
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.settings.PhoneFeature

private const val KEY_URL = "KEY_URL"
private const val KEY_IS_SECURED = "KEY_IS_SECURED"
private const val KEY_IS_SITE_IN_EXCEPTION_LIST = "KEY_IS_SITE_IN_EXCEPTION_LIST"
private const val REQUEST_CODE_QUICK_SETTINGS_PERMISSIONS = 4

@SuppressWarnings("TooManyFunctions")
class QuickSettingsSheetDialogFragment : BottomSheetDialogFragment() {
    private val safeArguments get() = requireNotNull(arguments)
    private val url: String by lazy { safeArguments.getString(KEY_URL) }
    private val isSecured: Boolean by lazy { safeArguments.getBoolean(KEY_IS_SECURED) }
    private val isSiteInExceptionList: Boolean by lazy { safeArguments.getBoolean(KEY_IS_SITE_IN_EXCEPTION_LIST) }
    private lateinit var quickSettingsComponent: QuickSettingsComponent

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_quick_settings_dialog_sheet, container, false)
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        quickSettingsComponent = QuickSettingsComponent(
            rootView as ConstraintLayout, ActionBusFactory.get(this),
            QuickSettingsState(
                QuickSettingsState.Mode.Normal(url, isSecured, isSiteInExceptionList)
            )
        )
    }

    companion object {
        const val FRAGMENT_TAG = "QUICK_SETTINGS_FRAGMENT_TAG"

        fun newInstance(
            url: String,
            isSecured: Boolean,
            isSiteInExceptionList: Boolean
        ): QuickSettingsSheetDialogFragment {

            val fragment = QuickSettingsSheetDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_URL, url)
                putBoolean(KEY_IS_SECURED, isSecured)
                putBoolean(KEY_IS_SITE_IN_EXCEPTION_LIST, isSiteInExceptionList)
            }
            fragment.arguments = arguments
            return fragment
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (arePermissionsGranted(requestCode, grantResults)) {
            val feature = requireNotNull(PhoneFeature.findFeatureBy(permissions))
            getManagedEmitter<QuickSettingsChange>()
                .onNext(QuickSettingsChange.PermissionGranted(feature))
        }
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
                    is QuickSettingsAction.DismissDialog -> dismiss()
                }
            }

        if (isVisible) {
            getManagedEmitter<QuickSettingsChange>()
                .onNext(QuickSettingsChange.PromptRestarted)
        }
    }
}
