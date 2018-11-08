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
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.newFixedThreadPoolContext
import mozilla.components.concept.awesomebar.AwesomeBar

private const val PROVIDER_QUERY_THREADS = 3

/**
 * A customizable [AwesomeBar] implementation.
 */
class BrowserAwesomeBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr), AwesomeBar {
    private val jobDispatcher = newFixedThreadPoolContext(PROVIDER_QUERY_THREADS, "AwesomeBarProviders")
    private val suggestionsAdapter = SuggestionsAdapter(this)
    private val providers: MutableList<AwesomeBar.SuggestionProvider> = mutableListOf()
    internal var scope = CoroutineScope(Dispatchers.Main)
    internal var job: Job? = null
    internal var listener: (() -> Unit)? = null

    init {
        layoutManager = LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false)
        adapter = suggestionsAdapter
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
                    val suggestions = async(jobDispatcher) { provider.onInputChanged(text) }
                    suggestionsAdapter.addSuggestions(provider, suggestions.await())
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
