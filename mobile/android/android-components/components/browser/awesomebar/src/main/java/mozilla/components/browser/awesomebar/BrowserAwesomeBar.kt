/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.content.Context
import android.util.AttributeSet
import android.util.LruCache
import androidx.annotation.MainThread
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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
internal const val PROVIDER_MAX_SUGGESTIONS = 20
internal const val INITIAL_NUMBER_OF_PROVIDERS = 5

/**
 * A customizable [AwesomeBar] implementation.
 */
@Suppress("TooManyFunctions")
class BrowserAwesomeBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr), AwesomeBar {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val jobDispatcher = Executors.newFixedThreadPool(PROVIDER_QUERY_THREADS).asCoroutineDispatcher()
    private val providers: MutableList<AwesomeBar.SuggestionProvider> = mutableListOf()
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val uniqueSuggestionIds = LruCache<String, Long>(INITIAL_NUMBER_OF_PROVIDERS * PROVIDER_MAX_SUGGESTIONS)
    private var lastUsedSuggestionId = 0L
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
        layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
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
        this.resizeUniqueSuggestionIdCache(this.providers.size)
    }

    @Synchronized
    override fun removeProviders(vararg providers: AwesomeBar.SuggestionProvider) {
        providers.forEach {
            it.onInputCancelled()
            suggestionsAdapter.removeSuggestions(it)
            this.providers.remove(it)
        }
        this.resizeUniqueSuggestionIdCache(this.providers.size)
    }

    @Synchronized
    override fun removeAllProviders() {
        val providerIterator = providers.iterator()

        while (providerIterator.hasNext()) {
            val provider = providerIterator.next()
            provider.onInputCancelled()
            suggestionsAdapter.removeSuggestions(provider)
            providerIterator.remove()
        }
        this.resizeUniqueSuggestionIdCache(0)
    }

    @Synchronized
    override fun onInputStarted() {
        queryProvidersForSuggestions { onInputStarted() }
    }

    @Synchronized
    override fun onInputChanged(text: String) {
        queryProvidersForSuggestions { onInputChanged(text) }
    }

    private fun queryProvidersForSuggestions(
        block: suspend AwesomeBar.SuggestionProvider.() -> List<AwesomeBar.Suggestion>
    ) {
        job?.cancel()

        job = scope.launch() {
            providers.forEach { provider ->
                launch {
                    val suggestions = withContext(jobDispatcher) { provider.block() }
                    val processedSuggestions = processProviderSuggestions(suggestions)
                    suggestionsAdapter.addSuggestions(
                        provider,
                        transformer?.transform(provider, processedSuggestions) ?: processedSuggestions
                    )
                }
            }
        }

        suggestionsAdapter.optionallyClearSuggestions()
    }

    internal fun processProviderSuggestions(suggestions: List<AwesomeBar.Suggestion>): List<AwesomeBar.Suggestion> {
        // We're generating unique suggestion IDs to guard against collisions
        // across providers, but we still require unique IDs for suggestions
        // from individual providers.
        val idSet = mutableSetOf<String>()
        suggestions.forEach {
            check(idSet.add(it.id)) { "${it.provider::class.java.simpleName} returned duplicate suggestion IDs" }
        }

        return suggestions.sortedByDescending { it.score }.take(PROVIDER_MAX_SUGGESTIONS)
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

    /**
     * Returns a unique suggestion ID to make sure ID's can't collide
     * across providers. This method is not thread-safe and must be
     * invoked on the main thread.
     *
     * @param suggestion the suggestion for which a unique ID should be
     * generated.
     *
     * @return the unique ID.
     */
    @MainThread
    fun getUniqueSuggestionId(suggestion: AwesomeBar.Suggestion): Long {
        val key = suggestion.provider.id + "/" + suggestion.id
        return uniqueSuggestionIds[key] ?: run {
            lastUsedSuggestionId += 1
            uniqueSuggestionIds.put(key, lastUsedSuggestionId)
            lastUsedSuggestionId
        }
    }

    private fun resizeUniqueSuggestionIdCache(providerCount: Int) {
        if (providerCount > 0) {
            this.uniqueSuggestionIds.resize((providerCount * 2) * PROVIDER_MAX_SUGGESTIONS)
        } else {
            this.uniqueSuggestionIds.evictAll()
        }
    }
}

internal data class BrowserAwesomeBarStyling(
    val titleTextColor: Int,
    val descriptionTextColor: Int,
    val chipTextColor: Int,
    val chipBackgroundColor: Int,
    val chipSpacing: Int
)
