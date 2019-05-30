/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.internal.loadLambda
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.WebURLFinder
import java.util.UUID

private const val MIME_TYPE_TEXT_PLAIN = "text/plain"

/**
 * An [AwesomeBar.SuggestionProvider] implementation that returns a suggestions for an URL in the clipboard (if there's
 * any).
 */
class ClipboardSuggestionProvider(
    context: Context,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icon: Bitmap? = null,
    private val title: String? = null,
    private val icons: BrowserIcons? = null,
    private val requireEmptyText: Boolean = true
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    private val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    override fun onInputStarted(): List<AwesomeBar.Suggestion> = createClipboardSuggestion()

    override suspend fun onInputChanged(text: String) =
            if ((requireEmptyText && text.isEmpty()) || !requireEmptyText) createClipboardSuggestion() else emptyList()

    private fun createClipboardSuggestion(): List<AwesomeBar.Suggestion> {
        val url = getTextFromClipboard(clipboardManager)?.let {
            findUrl(it)
        } ?: return emptyList()

        return listOf(AwesomeBar.Suggestion(
            provider = this,
            id = url,
            description = url,
            flags = setOf(AwesomeBar.Suggestion.Flag.CLIPBOARD),
            icon = icons.loadLambda(url, icon),
            title = title,
            onSuggestionClicked = {
                loadUrlUseCase.invoke(url)
            }
        ))
    }

    override val shouldClearSuggestions: Boolean
        // We do not want the suggestion of this provider to disappear and re-appear when text changes.
        get() = false
}

private fun findUrl(text: String): String? {
    val finder = WebURLFinder(text)
    return finder.bestWebURL()
}

private fun getTextFromClipboard(clipboardManager: ClipboardManager): String? {
    if (clipboardManager.isPrimaryClipEmpty() || !clipboardManager.isPrimaryClipPlainText()) {
        // We only care about a primary clip with type "text/plain"
        return null
    }

    return clipboardManager.firstPrimaryClipItem?.text?.toString()
}

private fun ClipboardManager.isPrimaryClipPlainText() =
    primaryClipDescription?.hasMimeType(MIME_TYPE_TEXT_PLAIN) ?: false

private fun ClipboardManager.isPrimaryClipEmpty() = primaryClip?.itemCount == 0

private val ClipboardManager.firstPrimaryClipItem: ClipData.Item?
    get() = primaryClip?.getItemAt(0)
