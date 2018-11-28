/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.support.annotation.VisibleForTesting
import android.support.v4.app.FragmentManager
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MULTIPLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.MENU_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.ChoiceDialogFragment.Companion.SINGLE_CHOICE_DIALOG_TYPE

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val FRAGMENT_TAG = "mozac_feature_prompt_dialog"

/**
 * Feature for displaying native dialogs for html elements like:
 * input type date,file,time,color, option, menu, authentication, confirmation and other alerts.
 *
 * This feature will subscribe to the currently selected [Session] and display a suitable native dialog based on
 * [Session.Observer.onPromptRequested] events. Once the dialog is closed or the user selects an item from the dialog
 * the related [PromptFeature] will be consumed.
 *
 * @property sessionManager The [SessionManager] instance in order to subscribe to the selected [Session].
 * @property fragmentManager The [FragmentManager] to be used when displaying a dialog (fragment).
 */
class PromptFeature(
    private val sessionManager: SessionManager,
    private val fragmentManager: FragmentManager
) {

    private val observer =
        PromptRequestObserver(sessionManager, feature = this)

    /**
     * Start observing the selected session and when needed show native dialogs.
     */
    fun start() {
        observer.observeSelected()

        fragmentManager.findFragmentByTag(FRAGMENT_TAG)?.let { fragment ->
            // There's still a [PromptDialogFragment] visible from the last time. Re-attach this feature so that the
            // fragment can invoke the callback on this feature once the user makes a selection. This can happen when
            // the app was in the background and on resume the activity and fragments get recreated.
            reattachFragment(fragment as PromptDialogFragment)
        }
    }

    /**
     * Stop observing the selected session incoming onPromptRequested events.
     */
    fun stop() {
        observer.stop()
    }

    /**
     * Event that is triggered when a native dialog needs to be shown.
     * Displays suitable dialog for the type of the [promptRequest].
     *
     * @param session The session which requested the dialog.
     * @param promptRequest The session the request the dialog.
     */
    internal fun onPromptRequested(session: Session, promptRequest: PromptRequest) {

        val dialog = when (promptRequest) {

            is SingleChoice -> {
                ChoiceDialogFragment.newInstance(
                    promptRequest.choices,
                    session.id, SINGLE_CHOICE_DIALOG_TYPE
                )
            }

            is MultipleChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MULTIPLE_CHOICE_DIALOG_TYPE
            )

            is MenuChoice -> ChoiceDialogFragment.newInstance(
                promptRequest.choices, session.id, MENU_CHOICE_DIALOG_TYPE
            )

            is Alert -> {
                with(promptRequest) {
                    AlertDialogFragment.newInstance(session.id, title, message, hasShownManyDialogs)
                }
            }
        }

        dialog.feature = this
        dialog.show(fragmentManager, FRAGMENT_TAG)
    }

    /**
     * Event that is triggered when a single choice or menu item is selected in a dialog.
     * @param sessionId this is the id of the session which requested the prompt.
     * @param choice the selection from the dialog.
     */
    internal fun onSingleChoiceSelect(sessionId: String, choice: Choice) {
        val session = sessionManager.findSessionById(sessionId) ?: return

        session.promptRequest.consume { request ->
            when (request) {
                is SingleChoice -> request.onSelect(choice)
                is MenuChoice -> request.onSelect(choice)
            }
            true
        }
    }

    /**
     * Event that is triggered when a multiple choice items are selected in a dialog.
     * @param sessionId this is the id of the session which requested the prompt.
     * @param choices the selected items from the dialog.
     */
    internal fun onMultipleChoiceSelect(sessionId: String, choices: Array<Choice>) {
        val session = sessionManager.findSessionById(sessionId) ?: return

        session.promptRequest.consume {
            (it as MultipleChoice).onSelect(choices)
            true
        }
    }

    /**
     * Event that is called when a dialog is dismissed.
     * This consumes the [PromptFeature] value from the [Session] indicated by [sessionId].
     * @param sessionId this is the id of the session which requested the prompt.
     */
    internal fun onCancel(sessionId: String) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.promptRequest.consume {
            when (it) {
                is Alert -> it.onDismiss()
            }
            true
        }
    }

    /**
     * Invoked when the user requested no more dialogs to be shown from this [sessionId].
     * This consumes the [PromptFeature] value from the [Session] indicated by [sessionId].
     * @param sessionId the id for which session the user doesn't want to show more dialogs.
     * @param isChecked tells if the user want to show more dialogs from this [sessionId].
     */
    internal fun onShouldMoreDialogsChecked(sessionId: String, isChecked: Boolean) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.promptRequest.consume {
            when (it) {
                is Alert -> it.onShouldShowNoMoreDialogs(isChecked)
            }
            true
        }
    }

    /**
     * Re-attach a fragment that is still visible but not linked to this feature anymore.
     */
    private fun reattachFragment(fragment: PromptDialogFragment) {
        val session = sessionManager.findSessionById(fragment.sessionId)
        if (session == null || session.promptRequest.isConsumed()) {
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
            return
        }
        // Re-assign the feature instance so that the fragment can invoke us once the user makes a selection or cancels
        // the dialog.
        fragment.feature = this
    }

    /**
     * Observes [Session.Observer.onPromptRequested] of the selected session and notifies the feature whenever a prompt
     * needs to be shown.
     */
    internal class PromptRequestObserver(
        sessionManager: SessionManager,
        private val feature: PromptFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onPromptRequested(session: Session, promptRequest: PromptRequest): Boolean {
            feature.onPromptRequested(session, promptRequest)
            return false
        }
    }
}
