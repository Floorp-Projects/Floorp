/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.support.annotation.VisibleForTesting
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AlertCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ButtonCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MENU
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_SINGLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ChoiceCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FileCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.TextCallback
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.ktx.kotlin.toDate
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_WEEK

typealias GeckoChoice = GeckoSession.PromptDelegate.Choice

/**
 * Gecko-based PromptDelegate implementation.
 */
@Suppress("TooManyFunctions")
internal class GeckoPromptDelegate(private val geckoEngineSession: GeckoEngineSession) :
    GeckoSession.PromptDelegate {

    override fun onChoicePrompt(
        session: GeckoSession?,
        title: String?,
        msg: String?,
        type: Int,
        geckoChoices: Array<out GeckoChoice>,
        callback: ChoiceCallback
    ) {

        val pair = convertToChoices(geckoChoices)
        // An array of all the GeckoChoices transformed as local Choices object.
        val choices = pair.first
        // A map that contains all local choices and map to GeckoChoices
        val mapChoicesToGeckoChoices = pair.second

        when (type) {

            CHOICE_TYPE_SINGLE -> {
                geckoEngineSession.notifyObservers {
                    onPromptRequest(SingleChoice(choices) { selectedChoice ->
                        val geckoChoice = mapChoicesToGeckoChoices[selectedChoice]
                        callback.confirm(geckoChoice)
                    })
                }
            }

            CHOICE_TYPE_MENU -> {
                geckoEngineSession.notifyObservers {
                    onPromptRequest(MenuChoice(choices) { selectedChoice ->
                        val geckoChoice = mapChoicesToGeckoChoices[selectedChoice]
                        callback.confirm(geckoChoice)
                    })
                }
            }

            CHOICE_TYPE_MULTIPLE -> {
                geckoEngineSession.notifyObservers {
                    onPromptRequest(MultipleChoice(choices) { choices ->
                        val ids = choices.toIdsArray()
                        callback.confirm(ids)
                    })
                }
            }
        }
    }

    override fun onAlert(
        session: GeckoSession?,
        title: String?,
        message: String?,
        callback: AlertCallback
    ) {

        val hasShownManyDialogs = callback.hasCheckbox()
        val onDismiss: () -> Unit = {
            callback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(Alert(title ?: "", message ?: "", hasShownManyDialogs, onDismiss) { showMoreDialogs ->
                callback.checkboxValue = showMoreDialogs
                callback.dismiss()
            })
        }
    }

    override fun onDateTimePrompt(
        session: GeckoSession?,
        title: String?,
        type: Int,
        value: String?,
        minDate: String?,
        maxDate: String?,
        geckoCallback: TextCallback
    ) {
        val initialDateString = value ?: ""
        val onClear: () -> Unit = {
            geckoCallback.confirm("")
        }
        when (type) {
            DATETIME_TYPE_DATE -> {
                notifyDatePromptRequest(
                    title ?: "",
                    initialDateString,
                    minDate,
                    maxDate,
                    onClear,
                    "yyyy-MM-dd",
                    geckoCallback
                )
            }
            DATETIME_TYPE_MONTH -> {
                notifyDatePromptRequest(
                    title ?: "",
                    initialDateString,
                    minDate,
                    maxDate,
                    onClear,
                    "yyyy-MM",
                    geckoCallback
                )
            }
            DATETIME_TYPE_WEEK -> {
                notifyDatePromptRequest(
                    title ?: "",
                    initialDateString,
                    minDate,
                    maxDate,
                    onClear,
                    "yyyy-'W'ww",
                    geckoCallback
                )
            }
            DATETIME_TYPE_TIME -> {
            }
            DATETIME_TYPE_DATETIME_LOCAL -> {
            }
        }
    }

    override fun onButtonPrompt(
        session: GeckoSession?,
        title: String?,
        msg: String?,
        btnMsg: Array<out String>?,
        callback: ButtonCallback?
    ) = Unit

    override fun onFilePrompt(
        session: GeckoSession?,
        title: String?,
        type: Int,
        mimeTypes: Array<out String>?,
        callback: FileCallback?
    ) = Unit // Related issue: https://github.com/mozilla-mobile/android-components/issues/1468

    override fun onColorPrompt(
        session: GeckoSession?,
        title: String?,
        value: String?,
        callback: TextCallback?
    ) = Unit // Related issue: https://github.com/mozilla-mobile/android-components/issues/1469

    override fun onAuthPrompt(
        session: GeckoSession?,
        title: String?,
        msg: String?,
        options: AuthOptions?,
        callback: AuthCallback?
    ) = Unit // Related issue: https://github.com/mozilla-mobile/android-components/issues/1378

    override fun onTextPrompt(
        session: GeckoSession?,
        title: String?,
        msg: String?,
        value: String?,
        callback: TextCallback?
    ) = Unit // Related issue: https://github.com/mozilla-mobile/android-components/issues/1471

    override fun onPopupRequest(session: GeckoSession?, targetUri: String?): GeckoResult<AllowOrDeny> {
        return GeckoResult()
    } // Related issue: https://github.com/mozilla-mobile/android-components/issues/1473

    private fun GeckoChoice.toChoice(): Choice {
        val choiceChildren = items?.map { it.toChoice() }?.toTypedArray()
        return Choice(id, !disabled, label ?: "", selected, separator, choiceChildren)
    }

    /**
     * Convert an array of [GeckoChoice] to [Choice].
     *
     * @property geckoChoices The ID of the option or group.
     * @return A [Pair] with all the [GeckoChoice] converted to [Choice]
     * and a map where you find is [GeckoChoice] representation .
     */
    private fun convertToChoices(
        geckoChoices: Array<out GeckoChoice>
    ): Pair<Array<Choice>, HashMap<Choice, GeckoChoice>> {

        val mapChoicesToGeckoChoices = HashMap<Choice, GeckoChoice>()

        val arrayOfChoices = geckoChoices.map { geckoChoice ->
            val choice = geckoChoice.toChoice()
            mapChoicesToGeckoChoices[choice] = geckoChoice
            choice
        }.toTypedArray()

        return Pair(arrayOfChoices, mapChoicesToGeckoChoices)
    }

    @Suppress("LongParameterList")
    private fun notifyDatePromptRequest(
        title: String,
        initialDateString: String,
        minDateString: String?,
        maxDateString: String?,
        onClear: () -> Unit,
        format: String,
        geckoCallback: TextCallback
    ) {
        val initialDate = initialDateString.toDate(format)
        val minDate = if (minDateString.isNullOrEmpty()) null else minDateString.toDate(format)
        val maxDate = if (maxDateString.isNullOrEmpty()) null else maxDateString.toDate(format)
        val onSelect: (Date) -> Unit = {
            val stringDate = it.toString(format)
            geckoCallback.confirm(stringDate)
        }
        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Date(title, initialDate, minDate, maxDate, onSelect, onClear)
            )
        }
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun Array<Choice>.toIdsArray(): Array<String> {
    return this.map { it.id }.toTypedArray()
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun Date.toString(format: String): String {
    val formatter = SimpleDateFormat(format, Locale.ROOT)
    return formatter.format(this) ?: ""
}
