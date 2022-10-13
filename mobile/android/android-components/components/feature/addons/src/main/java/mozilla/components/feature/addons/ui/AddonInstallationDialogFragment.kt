/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.annotation.SuppressLint
import android.app.Dialog
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.widget.AppCompatCheckBox
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.databinding.MozacFeatureAddonsFragmentDialogAddonInstalledBinding
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import java.io.IOException

@VisibleForTesting internal const val KEY_INSTALLED_ADDON = "KEY_ADDON"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
private const val KEY_CONFIRM_BUTTON_BACKGROUND_COLOR = "KEY_CONFIRM_BUTTON_BACKGROUND_COLOR"
private const val KEY_CONFIRM_BUTTON_TEXT_COLOR = "KEY_CONFIRM_BUTTON_TEXT_COLOR"
private const val KEY_CONFIRM_BUTTON_RADIUS = "KEY_CONFIRM_BUTTON_RADIUS"

@VisibleForTesting internal const val KEY_ICON = "KEY_ICON"

private const val DEFAULT_VALUE = Int.MAX_VALUE

/**
 * A dialog that shows [Addon] installation confirmation.
 */
class AddonInstallationDialogFragment : AppCompatDialogFragment() {
    private val scope = CoroutineScope(Dispatchers.IO)

    @VisibleForTesting internal var iconJob: Job? = null
    private val logger = Logger("AddonInstallationDialogFragment")

    /**
     * A lambda called when the confirm button is clicked.
     */
    var onConfirmButtonClicked: ((Addon, Boolean) -> Unit)? = null

    /**
     * Reference to the application's [AddonCollectionProvider] to fetch add-on icons.
     */
    var addonCollectionProvider: AddonCollectionProvider? = null

    private val safeArguments get() = requireNotNull(arguments)

    internal val addon get() = requireNotNull(safeArguments.getParcelable<Addon>(KEY_ADDON))
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

    override fun onStop() {
        super.onStop()
        iconJob?.cancel()
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

        rootView.findViewById<TextView>(R.id.title).text =
            requireContext().getString(
                R.string.mozac_feature_addons_installed_dialog_title,
                addon.translateName(requireContext()),
                requireContext().appName,
            )

        val icon = safeArguments.getParcelable<Bitmap>(KEY_ICON)
        if (icon != null) {
            binding.icon.setImageDrawable(BitmapDrawable(resources, icon))
        } else {
            iconJob = fetchIcon(addon, binding.icon)
        }

        val allowedInPrivateBrowsing = rootView.findViewById<AppCompatCheckBox>(R.id.allow_in_private_browsing)
        allowedInPrivateBrowsing.setOnCheckedChangeListener { _, isChecked ->
            allowPrivateBrowsing = isChecked
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun fetchIcon(addon: Addon, iconView: ImageView, scope: CoroutineScope = this.scope): Job {
        return scope.launch {
            try {
                val iconBitmap = addonCollectionProvider?.getAddonIconBitmap(addon)
                iconBitmap?.let {
                    scope.launch(Dispatchers.Main) {
                        safeArguments.putParcelable(KEY_ICON, it)
                        iconView.setImageDrawable(BitmapDrawable(iconView.resources, it))
                    }
                }
            } catch (e: IOException) {
                scope.launch(Dispatchers.Main) {
                    val context = iconView.context
                    val att = context.theme.resolveAttribute(android.R.attr.textColorPrimary)
                    iconView.setColorFilter(ContextCompat.getColor(context, att))
                    iconView.setImageDrawable(
                        ContextCompat.getDrawable(context, R.drawable.mozac_ic_extensions),
                    )
                }
                logger.error("Attempt to fetch the ${addon.id} icon failed", e)
            }
        }
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

    @Suppress("LongParameterList")
    companion object {
        /**
         * Returns a new instance of [AddonInstallationDialogFragment].
         * @param addon The addon to show in the dialog.
         * @param promptsStyling Styling properties for the dialog.
         * @param onConfirmButtonClicked A lambda called when the confirm button is clicked.
         */
        fun newInstance(
            addon: Addon,
            addonCollectionProvider: AddonCollectionProvider,
            promptsStyling: PromptsStyling? = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true,
            ),
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
            fragment.arguments = arguments
            fragment.addonCollectionProvider = addonCollectionProvider
            return fragment
        }
    }

    /**
     * Styling for the addon installation dialog.
     */
    data class PromptsStyling(
        val gravity: Int,
        val shouldWidthMatchParent: Boolean = false,
        @ColorRes
        val confirmButtonBackgroundColor: Int? = null,
        @ColorRes
        val confirmButtonTextColor: Int? = null,
        val confirmButtonRadius: Float? = null,
    )
}
