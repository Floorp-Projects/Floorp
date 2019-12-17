/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.Window
import android.widget.TextView
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout.LayoutParams
import androidx.annotation.ColorRes
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.ContextCompat
import mozilla.components.feature.addons.Addon
import org.mozilla.samples.browser.R

internal const val KEY_ADDON = "KEY_ADDON"
internal const val KEY_TITLE = "KEY_TITLE"
private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
private const val KEY_PERMISSIONS = "KEY_PERMISSIONS"
private const val KEY_POSITIVE_BUTTON_BACKGROUND_COLOR = "KEY_POSITIVE_BUTTON_BACKGROUND_COLOR"
private const val KEY_POSITIVE_BUTTON_TEXT_COLOR = "KEY_POSITIVE_BUTTON_TEXT_COLOR"
private const val KEY_POSITIVE_BUTTON_RADIUS = "KEY_POSITIVE_BUTTON_RADIUS"
private const val DEFAULT_VALUE = Int.MAX_VALUE

internal class PermissionsDialogFragment : AppCompatDialogFragment() {

    internal var onPositiveButtonClicked: ((Addon) -> Unit)? = null
    internal var onNegativeButtonClicked: (() -> Unit)? = null

    private val safeArguments get() = requireNotNull(arguments)

    internal val addon get() = safeArguments.getParcelable<Addon>(KEY_ADDON)

    internal val positiveButtonRadius
        get() =
            safeArguments.getFloat(KEY_POSITIVE_BUTTON_RADIUS, DEFAULT_VALUE.toFloat())

    internal val permissions: IntArray
        get() =
            safeArguments.getIntArray(KEY_PERMISSIONS) ?: intArrayOf()

    internal val title: String
        get() =
            safeArguments.getString(KEY_TITLE, "")

    internal val dialogGravity: Int
        get() =
            safeArguments.getInt(KEY_DIALOG_GRAVITY, DEFAULT_VALUE)
    internal val dialogShouldWidthMatchParent: Boolean
        get() =
            safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)

    internal val positiveButtonBackgroundColor
        get() =
            safeArguments.getInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, DEFAULT_VALUE)

    internal val positiveButtonTextColor
        get() =
            safeArguments.getInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, DEFAULT_VALUE)

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

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        onNegativeButtonClicked?.invoke()
    }

    private fun Dialog.setContainerView(rootView: View) {
        if (dialogShouldWidthMatchParent) {
            setContentView(rootView)
        } else {
            addContentView(
                rootView,
                LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.MATCH_PARENT
                )
            )
        }
    }

    @SuppressLint("InflateParams")
    private fun createContainer(): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.fragment_dialog_addon_permissions,
            null,
            false
        )
        rootView.findViewById<TextView>(R.id.title).text =
            requireContext().getString(R.string.addon_permissions_dialog_title, title)
        rootView.findViewById<TextView>(R.id.permissions).text = buildPermissionsText()

        val positiveButton = rootView.findViewById<Button>(R.id.allow_button)
        val negativeButton = rootView.findViewById<Button>(R.id.deny_button)

        positiveButton.setOnClickListener {
            onPositiveButtonClicked?.invoke(addon!!)
            dismiss()
        }

        if (positiveButtonBackgroundColor != DEFAULT_VALUE) {
            val backgroundTintList =
                ContextCompat.getColorStateList(requireContext(), positiveButtonBackgroundColor)
            positiveButton.backgroundTintList = backgroundTintList
        }

        if (positiveButtonTextColor != DEFAULT_VALUE) {
            val color = ContextCompat.getColor(requireContext(), positiveButtonTextColor)
            positiveButton.setTextColor(color)
        }

        if (positiveButtonRadius != DEFAULT_VALUE.toFloat()) {
            val shape = GradientDrawable()
            shape.shape = GradientDrawable.RECTANGLE
            shape.setColor(
                ContextCompat.getColor(
                    requireContext(),
                    positiveButtonBackgroundColor
                )
            )
            shape.cornerRadius = positiveButtonRadius
            positiveButton.background = shape
        }

        negativeButton.setOnClickListener {
            onNegativeButtonClicked?.invoke()
            dismiss()
        }

        return rootView
    }

    private fun buildPermissionsText(): String {
        var permissionsText = getString(R.string.addon_permissions_dialog_subtitle) + "\n\n"

        permissions.forEachIndexed { index, item ->
            val brakeLine = if (index + 1 != permissions.size) "\n\n" else ""
            val permissionText = requireContext().getString(item)
            permissionsText += "• $permissionText $brakeLine"
        }
        return permissionsText
    }

    @Suppress("LongParameterList")
    companion object {
        fun newInstance(
            addon: Addon,
            title: String,
            permissions: List<Int>,
            promptsStyling: PromptsStyling? = null,
            onPositiveButtonClicked: ((Addon) -> Unit)? = null,
            onNegativeButtonClicked: (() -> Unit)? = null
        ): PermissionsDialogFragment {

            val fragment = PermissionsDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            arguments.apply {
                putParcelable(KEY_ADDON, addon)
                putString(KEY_TITLE, title)
                putIntArray(KEY_PERMISSIONS, permissions.toIntArray())

                promptsStyling?.gravity?.apply {
                    putInt(KEY_DIALOG_GRAVITY, this)
                }
                promptsStyling?.shouldWidthMatchParent?.apply {
                    putBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT, this)
                }
                promptsStyling?.positiveButtonBackgroundColor?.apply {
                    putInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, this)
                }

                promptsStyling?.positiveButtonTextColor?.apply {
                    putInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, this)
                }
                println(permissions)
            }
            fragment.onPositiveButtonClicked = onPositiveButtonClicked
            fragment.onNegativeButtonClicked = onNegativeButtonClicked
            fragment.arguments = arguments
            return fragment
        }
    }

    /**
     * Styling for the permissions dialog.
     */
    data class PromptsStyling(
        val gravity: Int,
        val shouldWidthMatchParent: Boolean = false,
        @ColorRes
        val positiveButtonBackgroundColor: Int? = null,
        @ColorRes
        val positiveButtonTextColor: Int? = null,
        val positiveButtonRadius: Float? = null
    )
}
