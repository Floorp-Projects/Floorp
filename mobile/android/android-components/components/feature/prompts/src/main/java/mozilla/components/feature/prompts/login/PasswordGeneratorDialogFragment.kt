/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.support.utils.ext.getParcelableCompat

private const val GENERATED_PASSWORD = "GENERATED_PASSWORD"
private const val URL = "URL"

/**
 *  Defines a dialog for suggesting a strong generated password when creating a
 *  new account on a website
 */
internal class PasswordGeneratorDialogFragment : PromptDialogFragment() {

    private val generatedPassword: String? by lazy {
        safeArguments.getParcelableCompat(GENERATED_PASSWORD, String::class.java)!!
    }

    private val currentUrl: String? by lazy {
        safeArguments.getParcelableCompat(URL, String::class.java)!!
    }

    private var onSavedGeneratedPassword: () -> Unit = {}

    private var colorsProvider: PasswordGeneratorDialogColorsProvider =
        PasswordGeneratorDialogColors.defaultProvider()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return BottomSheetDialog(requireContext(), R.style.MozDialogStyle).apply {
            setCancelable(true)
            setOnShowListener {
                val bottomSheet =
                    findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.peekHeight = resources.displayMetrics.heightPixels
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
        setContent {
            val colors = if (isSystemInDarkTheme()) darkColors() else lightColors()
            MaterialTheme(colors) {
                if (generatedPassword != null && currentUrl != null) {
                    PasswordGeneratorBottomSheet(
                        generatedStrongPassword = generatedPassword!!,
                        onUsePassword = {
                            onUsePassword(
                                generatedPassword = generatedPassword!!,
                                currentUrl = currentUrl!!,
                            )
                        },
                        onCancelDialog = { onCancelDialog() },
                        colors = colorsProvider.provideColors(),
                    )
                }
            }
        }
    }

    /**
     * Called when a generated password is being used when creating a new account on a website.
     */
    @VisibleForTesting
    internal fun onUsePassword(generatedPassword: String, currentUrl: String) {
        val login = Login(
            guid = "",
            origin = currentUrl,
            formActionOrigin = currentUrl,
            httpRealm = currentUrl,
            username = "",
            password = generatedPassword,
        )
        feature?.onConfirm(sessionId, promptRequestUID, login)
        dismiss()

        onSavedGeneratedPassword.invoke()
    }

    @VisibleForTesting
    internal fun onCancelDialog() {
        feature?.onCancel(sessionId, promptRequestUID)
        dismiss()
    }

    companion object {

        /**
         * A builder method for creating a [PasswordGeneratorDialogFragment]
         * @param sessionId The id of the session for which this dialog will be created.
         * @param promptRequestUID Identifier of the PromptRequest for which this dialog is shown.
         * @param generatedPassword The strong generated password.
         * @param currentUrl The url for which the strong password is generated.
         * @param colorsProvider The color provider for the password generator bottom sheet.
         */
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            generatedPassword: String,
            currentUrl: String,
            onSavedGeneratedPassword: () -> Unit,
            colorsProvider: PasswordGeneratorDialogColorsProvider,
        ) = PasswordGeneratorDialogFragment().apply {
            arguments = (arguments ?: Bundle()).apply {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putString(GENERATED_PASSWORD, generatedPassword)
                putString(URL, currentUrl)
            }
            this.onSavedGeneratedPassword = onSavedGeneratedPassword
            this.colorsProvider = colorsProvider
        }
    }
}
