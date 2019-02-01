/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.content.Context
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.util.AttributeSet
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.awesomebar.layout.SuggestionLayout
import mozilla.components.browser.awesomebar.transform.SuggestionTransformer
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.ktx.android.content.res.pxToDp
import java.util.concurrent.Executors

private const val PROVIDER_QUERY_THREADS = 3

private const val DEFAULT_TITLE_TEXT_COLOR = 0xFF272727.toInt()
private const val DEFAULT_DESCRIPTION_TEXT_COLOR = 0xFF737373.toInt()
private const val DEFAULT_CHIP_TEXT_COLOR = 0xFF272727.toInt()
private const val DEFAULT_CHIP_BACKGROUND_COLOR = 0xFFEEEEEE.toInt()
private const val DEFAULT_CHIP_SPACING_DP = 2

/**
 * A customizable [AwesomeBar] implementation.
 */
class BrowserAwesomeBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr), AwesomeBar {
    private val jobDispatcher = Executors.newFixedThreadPool(PROVIDER_QUERY_THREADS).asCoroutineDispatcher()
    private val providers: MutableList<AwesomeBar.SuggestionProvider> = mutableListOf()
    internal var suggestionsAdapter = SuggestionsAdapter(this)
    internal var scope = CoroutineScope(Dispatchers.Main)
    internal var job: Job? = null
    internal var listener: (() -> Unit)? = null
    internal val styling: BrowserAwesomeBarStyling

    /**
     * The [SuggestionLayout] implementation controls layout inflation and view binding for suggestions.
     */
    var layout: SuggestionLayout
        get() = suggestionsAdapter.layout
        set(value) { suggestionsAdapter.layout = value }

    /**
     * An optional [SuggestionTransformer] that receives [AwesomeBar.Suggestion] objects from a
     * [AwesomeBar.SuggestionProvider] and returns a new list of transformed [AwesomeBar.Suggestion] objects.
     */
    var transformer: SuggestionTransformer? = null

    init {
        layoutManager = LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false)
        adapter = suggestionsAdapter

        val attr = context.obtainStyledAttributes(attrs, R.styleable.BrowserAwesomeBar, defStyleAttr, 0)
        this@BrowserAwesomeBar.styling = BrowserAwesomeBarStyling(
            attr.getColor(R.styleable.BrowserAwesomeBar_awesomeBarTitleTextColor, DEFAULT_TITLE_TEXT_COLOR),
            attr.getColor(R.styleable.BrowserAwesomeBar_awesomeBarDescriptionTextColor, DEFAULT_DESCRIPTION_TEXT_COLOR),
            attr.getColor(R.styleable.BrowserAwesomeBar_awesomeBarChipTextColor, DEFAULT_CHIP_TEXT_COLOR),
            attr.getColor(R.styleable.BrowserAwesomeBar_awesomeBarChipBackgroundColor, DEFAULT_CHIP_BACKGROUND_COLOR),
            attr.getDimensionPixelSize(R.styleable.BrowserAwesomeBar_awesomeBarChipSpacing, resources.pxToDp(
                DEFAULT_CHIP_SPACING_DP))
        )
        attr.recycle()
    }

    @Synchronized
    override fun addProviders(vararg providers: AwesomeBar.SuggestionProvider) {
        this.providers.addAll(providers)
    }

    @Synchronized
    override fun onInputStarted() {
        providers.forEach { provider -> provider.onInputStarted() }
    }

    @Synchronized
    override fun onInputChanged(text: String) {
        job?.cancel()

        suggestionsAdapter.clearSuggestions()

        job = scope.launch {
            providers.forEach { provider ->
                launch {
                    val suggestions = async(jobDispatcher) {
                        provider.onInputChanged(text)
                    }

                    suggestionsAdapter.addSuggestions(
                        provider,
                        transformer?.transform(provider, suggestions.await()) ?: suggestions.await()
                    )
                }
            }
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        job?.cancel()
        jobDispatcher.close()
    }

    @Synchronized
    override fun onInputCancelled() {
        job?.cancel()

        providers.forEach { provider -> provider.onInputCancelled() }
    }

    override fun setOnStopListener(listener: () -> Unit) {
        this.listener = listener
    }
}

internal data class BrowserAwesomeBarStyling(
    val titleTextColor: Int,
    val descriptionTextColor: Int,
    val chipTextColor: Int,
    val chipBackgroundColor: Int,
    val chipSpacing: Int
)
