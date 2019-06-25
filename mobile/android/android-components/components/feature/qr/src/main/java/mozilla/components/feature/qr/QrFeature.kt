/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.Manifest.permission.CAMERA
import android.content.Context
import androidx.annotation.MainThread
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.ktx.android.content.isPermissionGranted

typealias OnScanResult = (result: String) -> Unit

/**
 * Feature implementation that provides QR scanning functionality via the [QrFragment].
 *
 * @property context a reference to the context.
 * @property fragmentManager a reference to a [FragmentManager], used to start
 * the [QrFragment].
 * @property onScanResult a callback invoked with the result of the QR scan.
 * The callback will always be invoked on the main thread.
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested before a QR scan can be performed. Once the request
 * is completed, [onPermissionsResult] needs to be invoked. This feature
 * will request [android.Manifest.permission.CAMERA].
 */
class QrFeature(
    private val context: Context,
    private val fragmentManager: FragmentManager,
    private val onScanResult: OnScanResult = { },
    override val onNeedToRequestPermissions: OnNeedToRequestPermissions = { }
) : LifecycleAwareFeature, BackHandler, PermissionsFeature {
    private var containerViewId: Int = 0

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val scanCompleteListener: QrFragment.OnScanCompleteListener = object : QrFragment.OnScanCompleteListener {
        @MainThread
        override fun onScanComplete(result: String) {
            removeQrFragment()
            onScanResult(result)
        }
    }

    override fun start() {
        (fragmentManager.findFragmentByTag(QR_FRAGMENT_TAG) as? QrFragment)?.let {
            it.scanCompleteListener = scanCompleteListener
        }
    }

    override fun stop() {
        // Nothing to do here for now.
    }

    override fun onBackPressed(): Boolean {
        return removeQrFragment()
    }

    /**
     * Starts the QR scanner fragment and listens for scan results.
     *
     * @param containerViewId optional id of the container this fragment is to
     * be placed in, defaults to [android.R.id.content].
     *
     * @return true if the scanner was started or false if permissions still
     * need to be requested.
     */
    fun scan(containerViewId: Int = android.R.id.content): Boolean {
        this.containerViewId = containerViewId

        return if (context.isPermissionGranted(CAMERA)) {
            fragmentManager.beginTransaction()
                .add(containerViewId, QrFragment.newInstance(scanCompleteListener), QR_FRAGMENT_TAG)
                .commit()
            true
        } else {
            onNeedToRequestPermissions(arrayOf(CAMERA))
            false
        }
    }

    /**
     * Notifies the feature that the permission request was completed. If the
     * requested permissions were granted it will open the QR scanner.
     */
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (context.isPermissionGranted(CAMERA)) {
            scan(containerViewId)
        }
    }

    /**
     * Removes the QR fragment.
     *
     * @return true if the fragment was removed, otherwise false.
     */
    internal fun removeQrFragment(): Boolean {
        with(fragmentManager) {
            findFragmentByTag(QR_FRAGMENT_TAG)?.let {
                beginTransaction().remove(it).commit()
                return true
            }
        }
        return false
    }

    companion object {
        internal const val QR_FRAGMENT_TAG = "MOZAC_QR_FRAGMENT"
    }
}
