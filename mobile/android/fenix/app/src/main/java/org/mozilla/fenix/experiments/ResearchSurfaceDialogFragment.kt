/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import android.annotation.SuppressLint
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.DialogFragment
import androidx.navigation.fragment.navArgs
import org.mozilla.fenix.R
import org.mozilla.fenix.experiments.view.ResearchSurfaceSurvey
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialog displaying the fullscreen research surface message.
 */

class ResearchSurfaceDialogFragment : DialogFragment() {
    private val args by navArgs<ResearchSurfaceDialogFragmentArgs>()
    private lateinit var bundleArgs: Bundle

    /**
     * A callback to trigger the 'Take Survey' button of the dialog.
     */
    var onAccept: () -> Unit = {}

    /**
     * A callback to trigger the 'No Thanks' button of the dialog.
     */
    var onDismiss: () -> Unit = {}

    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.ResearchSurfaceDialogStyle)

        bundleArgs = args.toBundle()
    }

    override fun onDestroy() {
        super.onDestroy()
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        val messageText = bundleArgs.getString(KEY_MESSAGE_TEXT)
            ?: getString(R.string.nimbus_survey_message_text)
        val acceptButtonText = bundleArgs.getString(KEY_ACCEPT_BUTTON_TEXT)
            ?: getString(R.string.preferences_take_survey)
        val dismissButtonText = bundleArgs.getString(KEY_DISMISS_BUTTON_TEXT)
            ?: getString(R.string.preferences_not_take_survey)

        setContent {
            FirefoxTheme {
                ResearchSurfaceSurvey(
                    messageText = messageText,
                    onAcceptButtonText = acceptButtonText,
                    onDismissButtonText = dismissButtonText,
                    onDismiss = {
                        onDismiss()
                        dismiss()
                    },
                    onAccept = {
                        onAccept()
                        dismiss()
                    },
                )
            }
        }
    }

    companion object {
        /**
         * A builder method for creating a [ResearchSurfaceDialogFragment]
         */
        fun newInstance(
            keyMessageText: String?,
            keyAcceptButtonText: String?,
            keyDismissButtonText: String?,
        ): ResearchSurfaceDialogFragment {
            val fragment = ResearchSurfaceDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_MESSAGE_TEXT, keyMessageText)

                putString(KEY_ACCEPT_BUTTON_TEXT, keyAcceptButtonText)

                putString(KEY_DISMISS_BUTTON_TEXT, keyDismissButtonText)
            }

            fragment.arguments = arguments
            fragment.isCancelable = false

            return fragment
        }

        private const val KEY_MESSAGE_TEXT = "KEY_MESSAGE_TEXT"
        private const val KEY_ACCEPT_BUTTON_TEXT = "KEY_ACCEPT_BUTTON_TEXT"
        private const val KEY_DISMISS_BUTTON_TEXT = "KEY_DISMISS_BUTTON_TEXT"
        const val FRAGMENT_TAG = "MOZAC_RESEARCH_SURFACE_DIALOG_FRAGMENT"
    }
}
