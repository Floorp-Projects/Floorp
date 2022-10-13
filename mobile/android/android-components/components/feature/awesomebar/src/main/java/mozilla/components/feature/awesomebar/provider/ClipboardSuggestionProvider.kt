/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import android.os.Build
import android.view.textclassifier.TextClassifier
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.awesomebar.R
import mozilla.components.feature.awesomebar.facts.emitClipboardSuggestionClickedFact
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.WebURLFinder
import java.util.UUID

private const val MIME_TYPE_TEXT_PLAIN = "text/plain"
private const val MINIMUM_CONFIDENCE_SCORE_FOR_URL = 0.7F

/**
 * An [AwesomeBar.SuggestionProvider] implementation that returns a suggestions for an URL in the clipboard (if there's
 * any).
 *
 * @property context the activity or application context, required to look up the clipboard manager.
 * @property loadUrlUseCase the use case invoked to load the url when
 * the user clicks on the suggestion.
 * @property icon optional icon used for the [AwesomeBar.Suggestion].
 * @property title optional title used for the [AwesomeBar.Suggestion].
 * @property requireEmptyText whether or no the input text must be empty for a
 * clipboard suggestion to be provided, defaults to true.
 * @property engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 */
class ClipboardSuggestionProvider(
    private val context: Context,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icon: Bitmap? = null,
    private val title: String? = null,
    private val requireEmptyText: Boolean = true,
    internal val engine: Engine? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    private val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    override fun onInputStarted(): List<AwesomeBar.Suggestion> {
        return createClipboardSuggestion()
    }

    override suspend fun onInputChanged(text: String) =
        if ((requireEmptyText && text.isEmpty()) || !requireEmptyText) createClipboardSuggestion() else emptyList()

    private fun createClipboardSuggestion(): List<AwesomeBar.Suggestion> {
        val clipboardUrl = getUrlFromClipboard(clipboardManager)

        val url = clipboardUrl?.let { findUrl(it) } ?: return emptyList()
        engine?.speculativeConnect(url)

        return listOf(
            AwesomeBar.Suggestion(
                provider = this,
                id = url,
                description = url,
                editSuggestion = url,
                flags = setOf(AwesomeBar.Suggestion.Flag.CLIPBOARD),
                icon = icon ?: ContextCompat.getDrawable(context, R.drawable.mozac_ic_search)?.toBitmap(),
                title = title,
                onSuggestionClicked = {
                    loadUrlUseCase.invoke(url)
                    emitClipboardSuggestionClickedFact()
                },
            ),
        )
    }
}

private fun findUrl(text: String): String? {
    val finder = WebURLFinder(text)
    return finder.bestWebURL()
}

private fun getUrlFromClipboard(clipboardManager: ClipboardManager): String? {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        val description = clipboardManager.primaryClipDescription
        // An IllegalStateException may be thrown if the clipboard text is too long.
        val score =
            try {
                description?.getConfidenceScore(TextClassifier.TYPE_URL) ?: 0F
            } catch (e: IllegalStateException) {
                0F
            }
        return if (score >= MINIMUM_CONFIDENCE_SCORE_FOR_URL) {
            getTextFromClipboard(clipboardManager)
        } else {
            null
        }
    } else {
        return getTextFromClipboard(clipboardManager)
    }
}

private fun getTextFromClipboard(clipboardManager: ClipboardManager): String? {
    val primaryClip = clipboardManager.primaryClip

    if (primaryClip?.itemCount == 0 || !clipboardManager.isPrimaryClipPlainText()) {
        // We only care about a primary clip with type "text/plain"
        return null
    }

    return primaryClip?.firstClipItemText
}

private fun ClipboardManager.isPrimaryClipPlainText() =
    primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_PLAIN) ?: false

private val ClipData.firstClipItemText: String
    get() = getItemAt(0).text.toString()
