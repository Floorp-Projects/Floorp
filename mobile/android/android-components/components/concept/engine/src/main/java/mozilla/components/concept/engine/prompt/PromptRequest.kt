/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import android.content.Context
import android.net.Uri
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection.Type

/**
 * Value type that represents a request for showing a native dialog for prompt web content.
 *
 */
sealed class PromptRequest {
    /**
     * Value type that represents a request for a single choice prompt.
     * @property choices All the possible options.
     * @property onConfirm A callback indicating which option was selected.
     */
    data class SingleChoice(val choices: Array<Choice>, val onConfirm: (Choice) -> Unit) : PromptRequest()

    /**
     * Value type that represents a request for a multiple choice prompt.
     * @property choices All the possible options.
     * @property onConfirm A callback indicating witch options has been selected.
     */
    data class MultipleChoice(val choices: Array<Choice>, val onConfirm: (Array<Choice>) -> Unit) : PromptRequest()

    /**
     * Value type that represents a request for a menu choice prompt.
     * @property choices All the possible options.
     * @property onConfirm A callback indicating which option was selected.
     */
    data class MenuChoice(val choices: Array<Choice>, val onConfirm: (Choice) -> Unit) : PromptRequest()

    /**
     * Value type that represents a request for an alert prompt.
     * @property title of the dialog.
     * @property message the body of the dialog.
     * @property hasShownManyDialogs tells if this page has shown multiple prompts within a short period of time.
     * @property onDismiss callback to let the page know the user dismissed the dialog.
     * @property onConfirm tells the web page if it should continue showing alerts or not.
     */
    data class Alert(
        val title: String,
        val message: String,
        val hasShownManyDialogs: Boolean = false,
        override val onDismiss: () -> Unit,
        val onConfirm: (Boolean) -> Unit
    ) : PromptRequest(), Dismissible

    /**
     * Value type that represents a request for an alert prompt to enter a message.
     * @property title title of the dialog.
     * @property inputLabel the label of the field the user should fill.
     * @property inputValue the default value of the field.
     * @property hasShownManyDialogs tells if this page has shown multiple prompts within a short period of time.
     * @property onDismiss callback to let the page know the user dismissed the dialog.
     * @property onConfirm tells the web page if it should continue showing alerts or not.
     */
    data class TextPrompt(
        val title: String,
        val inputLabel: String,
        val inputValue: String,
        val hasShownManyDialogs: Boolean = false,
        override val onDismiss: () -> Unit,
        val onConfirm: (Boolean, String) -> Unit
    ) : PromptRequest(), Dismissible

    /**
     * Value type that represents a request for a date prompt for picking a year, month, and day.
     * @property title of the dialog.
     * @property initialDate date that dialog should be set by default.
     * @property minimumDate date allow to be selected.
     * @property maximumDate date allow to be selected.
     * @property type indicate which [Type] of selection de user wants.
     * @property onConfirm callback that is called when the date is selected.
     * @property onClear callback that is called when the user requests the picker to be clear up.
     */
    class TimeSelection(
        val title: String,
        val initialDate: java.util.Date,
        val minimumDate: java.util.Date?,
        val maximumDate: java.util.Date?,
        val type: Type = Type.DATE,
        val onConfirm: (java.util.Date) -> Unit,
        val onClear: () -> Unit
    ) : PromptRequest() {
        enum class Type {
            DATE, DATE_AND_TIME, TIME, MONTH
        }
    }

    /**
     * Value type that represents a request for a selecting one or multiple files.
     * @property mimeTypes a set of allowed mime types. Only these file types can be selected.
     * @property isMultipleFilesSelection true if the user can select more that one file false otherwise.
     * @property captureMode indicates if the local media capturing capabilities should be used,
     * such as the camera or microphone.
     * @property onSingleFileSelected callback to notify that the user has selected a single file.
     * @property onMultipleFilesSelected callback to notify that the user has selected multiple files.
     * @property onDismiss callback to notify that the user has canceled the file selection.
     */
    data class File(
        val mimeTypes: Array<out String>,
        val isMultipleFilesSelection: Boolean = false,
        val captureMode: FacingMode = FacingMode.NONE,
        val onSingleFileSelected: (Context, Uri) -> Unit,
        val onMultipleFilesSelected: (Context, Array<Uri>) -> Unit,
        val onDismiss: () -> Unit
    ) : PromptRequest() {

        /**
         * @deprecated Use the new primary constructor.
         */
        constructor(
            mimeTypes: Array<out String>,
            isMultipleFilesSelection: Boolean,
            onSingleFileSelected: (Context, Uri) -> Unit,
            onMultipleFilesSelected: (Context, Array<Uri>) -> Unit,
            onDismiss: () -> Unit
        ) : this(
            mimeTypes,
            isMultipleFilesSelection,
            FacingMode.NONE,
            onSingleFileSelected,
            onMultipleFilesSelected,
            onDismiss
        )

        enum class FacingMode {
            NONE, ANY, FRONT_CAMERA, BACK_CAMERA
        }
    }

    /**
     * Value type that represents a request for an authentication prompt.
     * For more related info take a look at
     * <a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication>MDN docs</a>
     * @property title of the dialog.
     * @property message the body of the dialog.
     * @property userName default value provide for this session.
     * @property password default value provide for this session.
     * @property method type of authentication,  valid values [Method.HOST] and [Method.PROXY].
     * @property level indicates the level of security of the authentication like [Level.NONE],
     * [Level.SECURED] and [Level.PASSWORD_ENCRYPTED].
     * @property onlyShowPassword indicates if the dialog should only include a password field.
     * @property previousFailed indicates if this request is the result of a previous failed attempt to login.
     * @property isCrossOrigin indicates if this request is from a cross-origin sub-resource.
     * @property onConfirm callback to indicate the user want to start the authentication flow.
     * @property onDismiss callback to indicate the user dismissed this request.
     */
    data class Authentication(
        val title: String,
        val message: String,
        val userName: String,
        val password: String,
        val method: Method,
        val level: Level,
        val onlyShowPassword: Boolean = false,
        val previousFailed: Boolean = false,
        val isCrossOrigin: Boolean = false,
        val onConfirm: (String, String) -> Unit,
        override val onDismiss: () -> Unit
    ) : PromptRequest(), Dismissible {

        enum class Level {
            NONE, PASSWORD_ENCRYPTED, SECURED
        }

        enum class Method {
            HOST, PROXY
        }
    }

    /**
     * Value type that represents a request for a selecting one or multiple files.
     * @property defaultColor true if the user can select more that one file false otherwise.
     * @property onConfirm callback to notify that the user has selected a color.
     * @property onDismiss callback to notify that the user has canceled the dialog.
     */
    data class Color(
        val defaultColor: String,
        val onConfirm: (String) -> Unit,
        override val onDismiss: () -> Unit
    ) : PromptRequest(), Dismissible

    /**
     * Value type that represents a request for showing a pop-pup prompt.
     * This occurs when content attempts to open a new window,
     * in a way that doesn't appear to be the result of user input.
     *
     * @property targetUri the uri that the page is trying to open.
     * @property onAllow callback to notify that the user wants to open the [targetUri].
     * @property onDeny callback to notify that the user doesn't want to open the [targetUri].
     */
    data class Popup(
        val targetUri: String,
        val onAllow: () -> Unit,
        val onDeny: () -> Unit
    ) : PromptRequest()

    /**
     * Value type that represents a request for showing a
     * <a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/confirm>confirm prompt</a>.
     *
     * The prompt can have up to three buttons, they could be positive, negative and neutral.
     *
     * @property title of the dialog.
     * @property message the body of the dialog.
     * @property hasShownManyDialogs tells if this page has shown multiple prompts within a short period of time.
     * @property positiveButtonTitle optional title for the positive button.
     * @property negativeButtonTitle optional title for the negative button.
     * @property neutralButtonTitle optional title for the neutral button.
     * @property onConfirmPositiveButton callback to notify that the user has clicked the positive button.
     * @property onConfirmNegativeButton callback to notify that the user has clicked the negative button.
     * @property onConfirmNeutralButton callback to notify that the user has clicked the neutral button.
     * @property onDismiss callback to notify that the user has canceled the dialog.
     */
    data class Confirm(
        val title: String,
        val message: String,
        val hasShownManyDialogs: Boolean = false,
        val positiveButtonTitle: String = "",
        val negativeButtonTitle: String = "",
        val neutralButtonTitle: String = "",
        val onConfirmPositiveButton: (Boolean) -> Unit,
        val onConfirmNegativeButton: (Boolean) -> Unit,
        val onConfirmNeutralButton: (Boolean) -> Unit,
        override val onDismiss: () -> Unit
    ) : PromptRequest(), Dismissible

    interface Dismissible {
        val onDismiss: () -> Unit
    }
}
