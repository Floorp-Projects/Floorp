/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.app.Dialog
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
import androidx.core.view.isVisible
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import com.google.android.material.R as MaterialR

@ObsoleteCoroutinesApi
@SuppressWarnings("TooManyFunctions")
class QuickSettingsSheetDialogFragment : AppCompatDialogFragment() {
    private lateinit var websiteInfoStore: WebsiteInfoStore
    private lateinit var websitePermissionsStore: WebsitePermissionsStore
    private lateinit var websiteTrackingProtectionStore: TrackingProtectionStore
    private lateinit var websiteInfoView: WebsiteInfoView
    private lateinit var websitePermissionsView: WebsitePermissionsView
    private lateinit var websiteTrackingProtectionView: TrackingProtectionView
    private val safeArguments get() = requireNotNull(arguments)
    private val promptGravity: Int by lazy { QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments).gravity }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {

        val args = QuickSettingsSheetDialogFragmentArgs.fromBundle(safeArguments)
        val rootView = inflateRootView(container)

        websitePermissionsStore = WebsitePermissionsStore.createStore(
            context!!, args.sitePermissions, Settings.getInstance(context!!)
        )
        websiteInfoStore = WebsiteInfoStore.createStore(args.url, args.isSecured)

        if (!FeatureFlags.etpCategories) {
            websiteTrackingProtectionStore =
                TrackingProtectionStore.createStore(
                    args.url,
                    args.isTrackingProtectionOn,
                    context!!.settings()
                )
            websiteTrackingProtectionView =
                TrackingProtectionView(rootView.trackingProtectionLayout)
        } else {
            rootView.trackingProtectionGroup.isVisible = false
        }

        websiteInfoView = WebsiteInfoView(rootView.websiteInfoLayout)
        websitePermissionsView = WebsitePermissionsView(rootView.websitePermissionsLayout)

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

        consumeFrom(websiteInfoStore) { websiteInfoView.update(it) }
        consumeFrom(websitePermissionsStore) { websitePermissionsView.update(it) }
        if (::websiteTrackingProtectionStore.isInitialized) {
            consumeFrom(websiteTrackingProtectionStore) { websiteTrackingProtectionView.update(it) }
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
}
