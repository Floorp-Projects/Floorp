/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.content.SharedPreferences
import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.text.TextUtilsCompat
import androidx.core.view.ViewCompat
import androidx.core.view.isVisible
import androidx.lifecycle.ViewModelProvider
import androidx.preference.PreferenceManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesFeature
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.GleanMetrics.BrowserSearch
import org.mozilla.focus.GleanMetrics.SearchBar
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentUrlinputBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.defaultSearchEngineName
import org.mozilla.focus.ext.isSearch
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.settings
import org.mozilla.focus.input.InputToolbarIntegration
import org.mozilla.focus.menu.home.HomeMenu
import org.mozilla.focus.menu.home.HomeMenuItem
import org.mozilla.focus.nimbus.FocusNimbus
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.ui.SearchSuggestionsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.topsites.DefaultTopSitesStorage.Companion.TOP_SITES_MAX_LIMIT
import org.mozilla.focus.topsites.DefaultTopSitesView
import org.mozilla.focus.topsites.TopSitesOverlay
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.OneShotOnPreDrawListener
import org.mozilla.focus.utils.SearchUtils
import org.mozilla.focus.utils.StatusBarUtils
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import java.util.Locale
import kotlin.coroutines.CoroutineContext

private const val TIP_ONE_CAROUSEL_POSITION = 1
private const val TIP_TWO_CAROUSEL_POSITION = 2
private const val TIP_THREE_CAROUSEL_POSITION = 3
private const val TIP_FOUR_CAROUSEL_POSITION = 4
private const val TIP_FIVE_CAROUSEL_POSITION = 5

class FocusCrashException : Exception()

/**
 * Fragment for displaying the URL input controls.
 */
// Refactoring the size and function count of this fragment is non-trivial at this point.
// Therefore we ignore those violations for now.
@Suppress("LargeClass", "TooManyFunctions")
class UrlInputFragment :
    BaseFragment(),
    View.OnClickListener,
    SharedPreferences.OnSharedPreferenceChangeListener,
    CoroutineScope {
    companion object {
        @JvmField
        val FRAGMENT_TAG = "url_input"

        private const val duckDuckGo = "DuckDuckGo"

        private val ARGUMENT_ANIMATION = "animation"
        private val ARGUMENT_SESSION_UUID = "sesssion_uuid"

        private val ANIMATION_BROWSER_SCREEN = "browser_screen"

        private val ANIMATION_DURATION = 200

        @JvmStatic
        fun createWithoutSession(): UrlInputFragment {
            val arguments = Bundle()

            val fragment = UrlInputFragment()
            fragment.arguments = arguments

            return fragment
        }

        @JvmStatic
        fun createWithTab(
            tabId: String
        ): UrlInputFragment {
            val arguments = Bundle()

            arguments.putString(ARGUMENT_SESSION_UUID, tabId)
            arguments.putString(ARGUMENT_ANIMATION, ANIMATION_BROWSER_SCREEN)

            val fragment = UrlInputFragment()
            fragment.arguments = arguments

            return fragment
        }
    }

    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main
    private val shippedDomainsProvider = ShippedDomainsProvider()
    private val customDomainsProvider = CustomDomainsProvider()
    private var _binding: FragmentUrlinputBinding? = null
    private val binding get() = _binding!!
    private lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    @Volatile
    private var isAnimating: Boolean = false

    var tab: TabSessionState? = null
        private set

    private val isOverlay: Boolean
        get() = tab != null

    private var isInitialized = false

    private val toolbarIntegration = ViewBoundFeatureWrapper<InputToolbarIntegration>()
    private val topSitesFeature = ViewBoundFeatureWrapper<TopSitesFeature>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        PreferenceManager.getDefaultSharedPreferences(context)
            .registerOnSharedPreferenceChangeListener(this)

        // Get session from session manager if there's a session UUID in the fragment's arguments
        arguments?.getString(ARGUMENT_SESSION_UUID)?.let { id ->
            tab = requireComponents.store.state.findTab(id)
        }
    }

    @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/4958
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProvider(this).get(SearchSuggestionsViewModel::class.java)

        childFragmentManager.beginTransaction()
            .replace(binding.searchViewContainer.id, SearchSuggestionsFragment.create())
            .commit()

        searchSuggestionsViewModel.selectedSearchSuggestion.observe(
            viewLifecycleOwner
        ) {
            val isSuggestion = searchSuggestionsViewModel.searchQuery.value != it
            it?.let {
                if (searchSuggestionsViewModel.alwaysSearch) {
                    onSearch(it, isSuggestion = false, alwaysSearch = true)
                } else {
                    onSearch(it, isSuggestion)
                }
                searchSuggestionsViewModel.clearSearchSuggestion()
            }
        }

        searchSuggestionsViewModel.autocompleteSuggestion.observe(viewLifecycleOwner) { text ->
            if (text != null) {
                searchSuggestionsViewModel.clearAutocompleteSuggestion()
                binding.browserToolbar.setSearchTerms(text)
            }
        }
    }

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        activity?.let {
            shippedDomainsProvider.initialize(it.applicationContext)
            customDomainsProvider.initialize(it.applicationContext)
        }

        StatusBarUtils.getStatusBarHeight(binding.landingLayout) {
            adjustViewToStatusBarHeight(it)
        }

        if (!isInitialized) {
            // Explicitly switching to "edit mode" here in order to focus the toolbar and select
            // all text in it. We only want to do this once per fragment.
            binding.browserToolbar.editMode()
            isInitialized = true
        }
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
        view?.hideKeyboard()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun updateTipsLabel() {
        val context = context ?: return
        val tips = context.components.tipManager.getAvailableTips()
        binding.homeTips.tipsAdapter.submitList(tips)
        updateTipsPosition()
    }

    /**
     * The pro tips should start from right to left if the language is read right to left
     * (example Persian)
     */
    private fun updateTipsPosition() {
        val isRightToLeftLanguage = TextUtilsCompat.getLayoutDirectionFromLocale(
            Locale.getDefault()
        ) == ViewCompat.LAYOUT_DIRECTION_RTL
        if (isRightToLeftLanguage) {
            binding.homeTips.scrollToPosition(binding.homeTips.tipsAdapter.itemCount - 1)
        }
    }

    private fun adjustViewToStatusBarHeight(statusBarHeight: Int) {
        val inputHeight = resources.getDimension(R.dimen.urlinput_height)

        binding.urlInputLayout.layoutParams.height = (inputHeight + statusBarHeight).toInt()

        val topMargin = (inputHeight + statusBarHeight).toInt()

        if (binding.searchViewContainer.layoutParams is ViewGroup.MarginLayoutParams) {
            val marginParams = binding.searchViewContainer.layoutParams as ViewGroup.MarginLayoutParams
            marginParams.topMargin = topMargin
        }

        if (binding.landingLayout.layoutParams is ViewGroup.MarginLayoutParams) {
            val marginParams = binding.landingLayout.layoutParams as ViewGroup.MarginLayoutParams
            marginParams.topMargin = topMargin
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentUrlinputBinding.inflate(inflater, container, false)

        binding.topSites.setContent {
            FocusTheme {
                TopSitesOverlay()
            }
        }

        return binding.root
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.browserToolbar.private = true

        toolbarIntegration.set(
            InputToolbarIntegration(
                binding.browserToolbar,
                fragment = this,
                shippedDomainsProvider = shippedDomainsProvider,
                customDomainsProvider = customDomainsProvider
            ),
            owner = this,
            view = binding.browserToolbar
        )

        topSitesFeature.set(
            feature = TopSitesFeature(
                view = DefaultTopSitesView(requireComponents.appStore),
                storage = requireComponents.topSitesStorage,
                config = {
                    TopSitesConfig(
                        totalSites = TOP_SITES_MAX_LIMIT,
                        frecencyConfig = null,
                        providerConfig = null
                    )
                }
            ),
            owner = this,
            view = view
        )

        binding.dismissView.setOnClickListener(this)

        OneShotOnPreDrawListener(binding.urlInputContainerView) {
            animateFirstDraw()
            true
        }

        if (isOverlay) {
            binding.landingLayout.visibility = View.GONE
        } else {
            binding.backgroundView.background = AppCompatResources.getDrawable(
                requireContext(),
                R.drawable.home_background
            )

            binding.dismissView.visibility = View.GONE

            binding.menuView.visibility = View.VISIBLE
        }

        tab?.let { tab ->
            binding.browserToolbar.url = if (tab.content.isSearch) {
                tab.content.searchTerms
            } else {
                tab.content.url
            }

            binding.searchViewContainer.visibility = View.GONE
            binding.menuView.visibility = View.GONE
        }

        binding.browserToolbar.editMode()
        setHomeMenu()
        if (!FocusNimbus.features.onboarding.value().isCfrEnabled) {
            updateTipsLabel()
        } else {
            binding.homeTips.visibility = View.GONE
        }
    }

    private fun setHomeMenu() {
        binding.menuView.menuBuilder = HomeMenu(requireContext()) { menuItem ->
            when (menuItem) {
                is HomeMenuItem.Help -> openHelpPage()
                is HomeMenuItem.Settings -> openSettingsPage()
            }
        }.getMenuBuilder()
    }

    private fun openHelpPage() {
        requireComponents.tabsUseCases.addTab(
            SupportUtils.HELP_URL,
            source = SessionState.Source.Internal.Menu,
            private = true
        )
    }

    private fun openSettingsPage() {
        requireComponents.appStore.dispatch(
            AppAction.OpenSettings(page = Screen.Settings.Page.Start)
        )
    }

    fun onBackPressed(): Boolean {
        if (isOverlay) {
            animateAndDismiss()
            return true
        }

        return false
    }

    override fun onStart() {
        super.onStart()

        activity?.let {
            if (requireContext().settings.isFirstRun()) return@onStart
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        if (newConfig.orientation != Configuration.ORIENTATION_UNDEFINED) {
            // Make sure we update the background for landscape / portrait orientations.
            binding.backgroundView.background = AppCompatResources.getDrawable(
                requireContext(),
                R.drawable.home_background
            )
        }
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @Suppress("ComplexMethod")
    override fun onClick(view: View) {
        if (view.id == R.id.dismissView) {
            handleDismiss()
        } else {
            throw IllegalStateException("Unhandled view in onClick()")
        }
    }

    private fun handleDismiss() {
        if (isOverlay) {
            animateAndDismiss()
        } else {
            // This is a bit hacky, but emulates what the legacy toolbar did before we replaced
            // it with `browser-toolbar`. First we clear the text and then we invoke the "text
            // changed" callback manually for this change.
            binding.browserToolbar.edit.updateUrl("", false)
            onTextChange("")
        }
    }

    private fun animateFirstDraw() {
        if (ANIMATION_BROWSER_SCREEN == arguments?.getString(ARGUMENT_ANIMATION)) {
            playVisibilityAnimation(false)
        }
    }

    private fun animateAndDismiss() {
        ThreadUtils.assertOnUiThread()

        if (isAnimating) {
            // We are already animating some state change. Ignore all other requests.
            return
        }

        // Don't allow any more clicks: dismissView is still visible until the animation ends,
        // but we don't want to restart animations and/or trigger hiding again (which could potentially
        // cause crashes since we don't know what state we're in). Ignoring further clicks is the simplest
        // solution, since dismissView is about to disappear anyway.
        binding.dismissView.isClickable = false

        if (ANIMATION_BROWSER_SCREEN == arguments?.getString(ARGUMENT_ANIMATION)) {
            playVisibilityAnimation(true)
        } else {
            dismiss()
        }
    }

    /**
     * This animation is quite complex. The 'reverse' flag controls whether we want to show the UI
     * (false) or whether we are going to hide it (true). Additionally the animation is slightly
     * different depending on whether this fragment is shown as an overlay on top of other fragments
     * or if it draws its own background.
     */
    // This method correctly triggers a complexity warning. This method is indeed very and too complex.
    // However refactoring it is not trivial at this point so we ignore the warning for now.
    @Suppress("ComplexMethod")
    private fun playVisibilityAnimation(reverse: Boolean) {
        if (isAnimating) {
            // We are already animating, let's ignore another request.
            return
        }

        isAnimating = true

        val xyOffset = (
            if (isOverlay)
                (binding.urlInputContainerView.layoutParams as FrameLayout.LayoutParams).bottomMargin
            else
                0
            ).toFloat()

        val width = binding.urlInputBackgroundView.width.toFloat()
        val height = binding.urlInputBackgroundView.height.toFloat()

        val widthScale = if (isOverlay)
            (width + 2 * xyOffset) / width
        else
            1f

        val heightScale = if (isOverlay)
            (height + 2 * xyOffset) / height
        else
            1f

        if (!reverse) {
            binding.urlInputBackgroundView.apply {
                pivotX = 0f
                pivotY = 0f
                scaleX = widthScale
                scaleY = heightScale
                translationX = -xyOffset
                translationY = -xyOffset
            }
        }

        // Let the URL input use the full width/height and then shrink to the actual size
        binding.urlInputBackgroundView.animate()
            .setDuration(ANIMATION_DURATION.toLong())
            .scaleX(if (reverse) widthScale else 1f)
            .scaleY(if (reverse) heightScale else 1f)
            .alpha((if (reverse && isOverlay) 0 else 1).toFloat())
            .translationX(if (reverse) -xyOffset else 0f)
            .translationY(if (reverse) -xyOffset else 0f)
            .setListener(object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: Animator) {
                    if (reverse) {
                        if (isOverlay) {
                            dismiss()
                        }
                    }

                    isAnimating = false
                }
            })

        // We only need to animate the toolbar if we are an overlay.
        /*
        if (isOverlay) {
            val screenLocation = IntArray(2)
            //urlView?.getLocationOnScreen(screenLocation)

            val leftDelta = requireArguments().getInt(ARGUMENT_X) - screenLocation[0] - urlView.paddingLeft

            if (!reverse) {
                //urlView?.pivotX = 0f
                //urlView?.pivotY = 0f
                //urlView?.translationX = leftDelta.toFloat()
            }

            if (urlView != null) {
                // The URL moves from the right (at least if the lock is visible) to it's actual position
                urlView.animate()
                    .setDuration(ANIMATION_DURATION.toLong())
                    .translationX((if (reverse) leftDelta else 0).toFloat())
            }
        }
        */

        if (reverse) {
            binding.toolbarBottomBorder.visibility = View.VISIBLE

            if (!isOverlay) {
                binding.dismissView.visibility = View.GONE
                binding.menuView.visibility = View.VISIBLE
            }
        } else {
            binding.toolbarBottomBorder.visibility = View.GONE
        }
    }

    private fun dismiss() {
        // This method is called from animation callbacks. In the short time frame between the animation
        // starting and ending the activity can be paused. In this case this code can throw an
        // IllegalStateException because we already saved the state (of the activity / fragment) before
        // this transaction is committed. To avoid this we commit while allowing a state loss here.
        // We do not save any state in this fragment (It's getting destroyed) so this should not be a problem.

        requireComponents.appStore.dispatch(AppAction.FinishEdit(tab!!.id))
    }

    internal fun onCommit(input: String) {
        if (input.trim { it <= ' ' }.isNotEmpty()) {
            handleCrashTrigger(input)

            ViewUtils.hideKeyboard(binding.browserToolbar)

            if (handleL10NTrigger(input)) return

            val (isUrl, url, searchTerms) = normalizeUrlAndSearchTerms(input)

            openUrl(url, searchTerms)

            TelemetryWrapper.urlBarEvent(isUrl)

            if (isUrl) {
                SearchBar.enteredUrl.record(NoExtras())
            } else {
                val defaultSearchEngineName = requireComponents.store.defaultSearchEngineName()
                SearchBar.performedSearch.record(
                    SearchBar.PerformedSearchExtra(defaultSearchEngineName)
                )
                TelemetryWrapper.searchEnterEvent()
                BrowserSearch.searchCount["action"].add()
            }
        }
    }

    // This isn't that complex, and is only used for l10n screenshots.
    @Suppress("ComplexMethod")
    private fun handleL10NTrigger(input: String): Boolean {
        if (!AppConstants.isDevBuild) return false

        var triggerHandled = true

        when (input) {
            "l10n:tip:1" -> binding.homeTips.scrollToPosition(TIP_ONE_CAROUSEL_POSITION)
            "l10n:tip:2" -> binding.homeTips.scrollToPosition(TIP_TWO_CAROUSEL_POSITION)
            "l10n:tip:3" -> binding.homeTips.scrollToPosition(TIP_THREE_CAROUSEL_POSITION)
            "l10n:tip:4" -> binding.homeTips.scrollToPosition(TIP_FOUR_CAROUSEL_POSITION)
            "l10n:tip:5" -> binding.homeTips.scrollToPosition(TIP_FIVE_CAROUSEL_POSITION)
            else -> triggerHandled = false
        }

        if (triggerHandled) {
            binding.browserToolbar.displayMode()
            binding.browserToolbar.editMode()
        }
        return triggerHandled
    }

    private fun handleCrashTrigger(input: String) {
        if (input == "focus:crash") {
            throw FocusCrashException()
        }
    }

    private fun normalizeUrlAndSearchTerms(input: String): Triple<Boolean, String, String?> {
        val isUrl = UrlUtils.isUrl(input)

        val url = if (isUrl)
            UrlUtils.normalize(input)
        else
            SearchUtils.createSearchUrl(context, input)

        val searchTerms = if (isUrl)
            null
        else
            input.trim { it <= ' ' }
        return Triple(isUrl, url, searchTerms)
    }

    private fun onSearch(query: String, isSuggestion: Boolean = false, alwaysSearch: Boolean = false) {
        if (alwaysSearch) {
            val url = SearchUtils.createSearchUrl(context, query)
            openUrl(url, query)
        } else {
            val searchTerms = if (UrlUtils.isUrl(query)) null else query
            val searchUrl = if (searchTerms != null) {
                SearchUtils.createSearchUrl(context, searchTerms)
            } else {
                UrlUtils.normalize(query)
            }

            openUrl(searchUrl, searchTerms)
        }

        TelemetryWrapper.searchSelectEvent(isSuggestion)
        BrowserSearch.searchCount["suggestion"].add()
    }

    private fun openUrl(url: String, searchTerms: String?) {
        when (url) {
            "focus:about" -> {
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(Screen.Settings.Page.About)
                )
                return
            }
        }

        if (!searchTerms.isNullOrEmpty()) {
            tab?.let {
                requireComponents.store.dispatch(ContentAction.UpdateSearchTermsAction(it.id, searchTerms))
            }
        }

        val tab = tab
        if (tab != null) {
            requireComponents.sessionUseCases.loadUrl(url, tab.id)

            requireComponents.appStore.dispatch(AppAction.FinishEdit(tab.id))
        } else {
            val tabId = requireComponents.tabsUseCases.addTab(
                url,
                source = SessionState.Source.Internal.UserEntered,
                selectTab = true,
                private = true
            )

            if (!searchTerms.isNullOrEmpty()) {
                requireComponents.store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, searchTerms))
            }
        }
    }

    internal fun onStartEditing() {
        if (tab != null) {
            binding.searchViewContainer.isVisible = true
        }
    }

    internal fun onCancelEditing() {
        handleDismiss()
    }

    internal fun onTextChange(text: String) {
        searchSuggestionsViewModel.setSearchQuery(text)

        if (text.trim { it <= ' ' }.isEmpty()) {
            binding.searchViewContainer.visibility = View.GONE

            if (!isOverlay) {
                playVisibilityAnimation(true)
            }
        } else {
            binding.menuView.visibility = View.GONE

            if (!isOverlay && binding.dismissView.visibility != View.VISIBLE) {
                playVisibilityAnimation(false)
                binding.dismissView.visibility = View.VISIBLE
            }

            binding.searchViewContainer.visibility = View.VISIBLE
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        if (key == activity?.getString(R.string.pref_key_homescreen_tips)) { updateTipsLabel() }
    }
}
