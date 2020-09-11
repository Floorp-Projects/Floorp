/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.Settings
import android.text.SpannableString
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.navigation.fragment.findNavController
import androidx.preference.PreferenceManager
import mozilla.components.feature.qr.QrFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar

class PairFragment : Fragment(R.layout.fragment_pair), UserInteractionHandler {

    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        qrFeature.set(
            QrFeature(
                requireContext(),
                fragmentManager = parentFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
                },
                onScanResult = { pairingUrl ->
                    requireComponents.services.accountsAuthFeature.beginPairingAuthentication(
                        requireContext(),
                        pairingUrl
                    )
                    val vibrator = requireContext().getSystemService<Vibrator>()!!
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        vibrator.vibrate(
                            VibrationEffect.createOneShot(
                                VIBRATE_LENGTH,
                                VibrationEffect.DEFAULT_AMPLITUDE
                            )
                        )
                    } else {
                        @Suppress("Deprecation")
                        vibrator.vibrate(VIBRATE_LENGTH)
                    }
                    findNavController(this@PairFragment).popBackStack(
                        R.id.turnOnSyncFragment,
                        false
                    )
                },
            scanMessage = R.string.pair_instructions_2),
            owner = this,
            view = view
        )

        val cameraPermissionsDenied = preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_camera_permissions),
            false
        )

        qrFeature.withFeature {
            if (cameraPermissionsDenied) {
                showPermissionsNeededDialog()
            } else {
                it.scan(R.id.pair_layout)
            }
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.sync_scan_code))
    }

    override fun onBackPressed(): Boolean {
        qrFeature.onBackPressed()
        findNavController().popBackStack(R.id.turnOnSyncFragment, false)
        return true
    }

    companion object {
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
        private const val VIBRATE_LENGTH = 200L
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        when (requestCode) {
            REQUEST_CODE_CAMERA_PERMISSIONS -> {
                if (ContextCompat.checkSelfPermission(
                        requireContext(),
                        android.Manifest.permission.CAMERA
                    ) == PackageManager.PERMISSION_GRANTED
                ) {
                    qrFeature.withFeature {
                        it.onPermissionsResult(permissions, grantResults)
                    }
                    preferences.edit().putBoolean(
                        getPreferenceKey(R.string.pref_key_camera_permissions), false
                    ).apply()
                } else {
                    preferences.edit().putBoolean(
                        getPreferenceKey(R.string.pref_key_camera_permissions), true
                    ).apply()
                    findNavController().popBackStack(R.id.turnOnSyncFragment, false)
                }
            }
        }
    }

    /**
     * Shows an [AlertDialog] when camera permissions are needed.
     *
     * In versions above M, [AlertDialog.BUTTON_POSITIVE] takes the user to the app settings. This
     * intent only exists in M and above. Below M, [AlertDialog.BUTTON_POSITIVE] routes to a SUMO
     * help page to find the app settings.
     *
     * [AlertDialog.BUTTON_NEGATIVE] dismisses the dialog.
     */
    private fun showPermissionsNeededDialog() {
        AlertDialog.Builder(requireContext()).apply {
            val spannableText = SpannableString(
                resources.getString(R.string.camera_permissions_needed_message)
            )
            setMessage(spannableText)
            setNegativeButton(R.string.camera_permissions_needed_negative_button_text) {
                    dialog: DialogInterface, _ ->
                dialog.cancel()
            }
            setPositiveButton(R.string.camera_permissions_needed_positive_button_text) {
                    dialog: DialogInterface, _ ->
                val intent: Intent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                } else {
                    SupportUtils.createCustomTabIntent(
                        requireContext(),
                        SupportUtils.getSumoURLForTopic(
                            requireContext(),
                            SupportUtils.SumoTopic.QR_CAMERA_ACCESS
                        )
                    )
                }
                dialog.cancel()
                startActivity(intent)
            }
            create()
        }.show()
    }
}
