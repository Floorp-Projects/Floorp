/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatCheckBox
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentManager
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.databinding.MozacFeatureAddonsFragmentDialogAddonInstalledBinding
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.utils.ext.getParcelableCompat

@VisibleForTesting internal const val KEY_INSTALLED_ADDON = "KEY_INSTALLED_ADDON"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
private const val KEY_CONFIRM_BUTTON_BACKGROUND_COLOR = "KEY_CONFIRM_BUTTON_BACKGROUND_COLOR"
private const val KEY_CONFIRM_BUTTON_TEXT_COLOR = "KEY_CONFIRM_BUTTON_TEXT_COLOR"
private const val KEY_CONFIRM_BUTTON_RADIUS = "KEY_CONFIRM_BUTTON_RADIUS"

private const val DEFAULT_VALUE = Int.MAX_VALUE

/**
 * A dialog that shows [Addon] installation confirmation.
 */
class AddonInstallationDialogFragment : AddonDialogFragment() {
    /**
     * A lambda called when the confirm button is clicked.
     */
    var onConfirmButtonClicked: ((Addon, Boolean) -> Unit)? = null

    /**
     * A lambda called when the dialog is dismissed.
     */
    var onDismissed: (() -> Unit)? = null

    internal val addon get() = requireNotNull(safeArguments.getParcelableCompat(KEY_INSTALLED_ADDON, Addon::class.java))
    private var allowPrivateBrowsing: Boolean = false

    internal val confirmButtonRadius
        get() =
            safeArguments.getFloat(KEY_CONFIRM_BUTTON_RADIUS, DEFAULT_VALUE.toFloat())

    internal val dialogGravity: Int
        get() =
            safeArguments.getInt(
                KEY_DIALOG_GRAVITY,
                DEFAULT_VALUE,
            )
    internal val dialogShouldWidthMatchParent: Boolean
        get() =
            safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)

    internal val confirmButtonBackgroundColor
        get() =
            safeArguments.getInt(
                KEY_CONFIRM_BUTTON_BACKGROUND_COLOR,
                DEFAULT_VALUE,
            )

    internal val confirmButtonTextColor
        get() =
            safeArguments.getInt(
                KEY_CONFIRM_BUTTON_TEXT_COLOR,
                DEFAULT_VALUE,
            )

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        onDismissed?.invoke()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val sheetDialog = Dialog(requireContext())
        sheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        sheetDialog.setCanceledOnTouchOutside(true)

        val rootView = createContainer()

        sheetDialog.setContainerView(rootView)

        sheetDialog.window?.apply {
            if (dialogGravity != DEFAULT_VALUE) {
                setGravity(dialogGravity)
            }

            if (dialogShouldWidthMatchParent) {
                setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                // This must be called after addContentView, or it won't fully fill to the edge.
                setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            }
        }

        return sheetDialog
    }

    private fun Dialog.setContainerView(rootView: View) {
        if (dialogShouldWidthMatchParent) {
            setContentView(rootView)
        } else {
            addContentView(
                rootView,
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.MATCH_PARENT,
                ),
            )
        }
    }

    @SuppressLint("InflateParams")
    private fun createContainer(): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_feature_addons_fragment_dialog_addon_installed,
            null,
            false,
        )

        val binding = MozacFeatureAddonsFragmentDialogAddonInstalledBinding.bind(rootView)

        val addonName = addon.translateName(requireContext())
        rootView.findViewById<TextView>(R.id.title).text =
            requireContext().getString(
                R.string.mozac_feature_addons_installed_dialog_title,
                addonName,
                requireContext().appName,
            )
        rootView.findViewById<TextView>(R.id.description).text =
            requireContext().getString(
                R.string.mozac_feature_addons_installed_dialog_description_2,
                addonName,
                requireContext().appName,
            )

        loadIcon(addon = addon, iconView = binding.icon)

        val allowedInPrivateBrowsing = rootView.findViewById<AppCompatCheckBox>(R.id.allow_in_private_browsing)
        if (addon.incognito == Addon.Incognito.NOT_ALLOWED) {
            allowedInPrivateBrowsing.isVisible = false
        } else {
            allowedInPrivateBrowsing.setOnCheckedChangeListener { _, isChecked ->
                allowPrivateBrowsing = isChecked
            }
        }

        val confirmButton = rootView.findViewById<Button>(R.id.confirm_button)
        confirmButton.setOnClickListener {
            onConfirmButtonClicked?.invoke(addon, allowPrivateBrowsing)
            dismiss()
        }

        if (confirmButtonBackgroundColor != DEFAULT_VALUE) {
            val backgroundTintList =
                ContextCompat.getColorStateList(requireContext(), confirmButtonBackgroundColor)
            confirmButton.backgroundTintList = backgroundTintList
        }

        if (confirmButtonTextColor != DEFAULT_VALUE) {
            val color = ContextCompat.getColor(requireContext(), confirmButtonTextColor)
            confirmButton.setTextColor(color)
        }

        if (confirmButtonRadius != DEFAULT_VALUE.toFloat()) {
            val shape = GradientDrawable()
            shape.shape = GradientDrawable.RECTANGLE
            shape.setColor(
                ContextCompat.getColor(
                    requireContext(),
                    confirmButtonBackgroundColor,
                ),
            )
            shape.cornerRadius = confirmButtonRadius
            confirmButton.background = shape
        }

        return rootView
    }

    override fun show(manager: FragmentManager, tag: String?) {
        // This dialog is shown as a result of an async operation (installing
        // an add-on). Once installation succeeds, the activity may already be
        // in the process of being destroyed. Since the dialog doesn't have any
        // state we need to keep, and since it's also fine to not display the
        // dialog at all in case the user navigates away, we can simply use
        // commitAllowingStateLoss here to prevent crashing on commit:
        // https://github.com/mozilla-mobile/android-components/issues/7782
        val ft = manager.beginTransaction()
        ft.add(this, tag)
        ft.commitAllowingStateLoss()
    }

    companion object {
        /**
         * Returns a new instance of [AddonInstallationDialogFragment].
         * @param addon The addon to show in the dialog.
         * @param promptsStyling Styling properties for the dialog.
         * @param onDismissed A lambda called when the dialog is dismissed.
         * @param onConfirmButtonClicked A lambda called when the confirm button is clicked.
         */
        fun newInstance(
            addon: Addon,
            promptsStyling: PromptsStyling? = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true,
            ),
            onDismissed: (() -> Unit)? = null,
            onConfirmButtonClicked: ((Addon, Boolean) -> Unit)? = null,
        ): AddonInstallationDialogFragment {
            val fragment = AddonInstallationDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            arguments.apply {
                putParcelable(KEY_INSTALLED_ADDON, addon)

                promptsStyling?.gravity?.apply {
                    putInt(KEY_DIALOG_GRAVITY, this)
                }
                promptsStyling?.shouldWidthMatchParent?.apply {
                    putBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT, this)
                }
                promptsStyling?.confirmButtonBackgroundColor?.apply {
                    putInt(KEY_CONFIRM_BUTTON_BACKGROUND_COLOR, this)
                }

                promptsStyling?.confirmButtonTextColor?.apply {
                    putInt(KEY_CONFIRM_BUTTON_TEXT_COLOR, this)
                }
            }
            fragment.onConfirmButtonClicked = onConfirmButtonClicked
            fragment.onDismissed = onDismissed
            fragment.arguments = arguments
            return fragment
        }
    }
}
