/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.drawable.TransitionDrawable
import android.os.Bundle
import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.TextView
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.preference.PreferenceManager
import kotlinx.android.synthetic.main.fragment_urlinput2.*
import kotlinx.android.synthetic.main.fragment_urlinput2.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesFeature
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.R
import org.mozilla.focus.ext.isSearch
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.home.HomeScreen
import org.mozilla.focus.input.InputToolbarIntegration
import org.mozilla.focus.menu.home.HomeMenu
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.ui.SearchSuggestionsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.tips.Tip
import org.mozilla.focus.tips.TipManager
import org.mozilla.focus.topsites.DefaultTopSitesView
import org.mozilla.focus.topsites.TopSiteMenuItem
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.Features
import org.mozilla.focus.utils.OneShotOnPreDrawListener
import org.mozilla.focus.utils.SearchUtils
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.StatusBarUtils
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import kotlin.coroutines.CoroutineContext

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
        private val TIPS_ALPHA = 0.65f

        private lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

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

        /**
         * Create a new UrlInputFragment with a gradient background (and the Focus logo). This configuration
         * is usually shown if there's no content to be shown below (e.g. the current website).
         */
        @JvmStatic
        fun createWithBackground(): UrlInputFragment {
            val arguments = Bundle()

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
    private var displayedPopupMenu: HomeMenu? = null

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

        searchSuggestionsViewModel = ViewModelProvider(requireActivity()).get(SearchSuggestionsViewModel::class.java)

        childFragmentManager.beginTransaction()
            .replace(searchViewContainer.id, SearchSuggestionsFragment.create())
            .commit()

        searchSuggestionsViewModel.selectedSearchSuggestion.observe(
            viewLifecycleOwner,
            Observer {
                val isSuggestion = searchSuggestionsViewModel.searchQuery.value != it
                it?.let {
                    if (searchSuggestionsViewModel.alwaysSearch) {
                        onSearch(it, false, true)
                    } else {
                        onSearch(it, isSuggestion)
                    }
                    searchSuggestionsViewModel.clearSearchSuggestion()
                }
            }
        )
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

        StatusBarUtils.getStatusBarHeight(keyboardLinearLayout) {
            adjustViewToStatusBarHeight(it)
        }

        if (!isInitialized) {
            // Explicitly switching to "edit mode" here in order to focus the toolbar and select
            // all text in it. We only want to do this once per fragment.
            browserToolbar.editMode()
            isInitialized = true
        }
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
        view?.hideKeyboard()
    }

    private fun updateTipsLabel() {
        val context = context ?: return
        val tip = TipManager.getNextTipIfAvailable(context)
        updateSubtitle(tip)
    }

    private fun updateSubtitle(tip: Tip?) {
        if (tip == null) {
            showFocusSubtitle()
            return
        }

        val tipText = String.format(tip.text, System.getProperty("line.separator"))
        keyboardLinearLayout.homeViewTipsLabel.alpha = TIPS_ALPHA

        if (tip.deepLink == null) {
            homeViewTipsLabel.text = tipText
            return
        }

        // Only make the second line clickable if applicable
        val linkStartIndex =
            if (tipText.contains("\n")) tipText.indexOf("\n") + 2
            else 0

        keyboardLinearLayout.homeViewTipsLabel.movementMethod = LinkMovementMethod()
        homeViewTipsLabel.setText(tipText, TextView.BufferType.SPANNABLE)

        val deepLinkAction = object : ClickableSpan() {
            override fun onClick(p0: View) {
                tip.deepLink.invoke()
            }
        }

        val textWithDeepLink = SpannableString(tipText).apply {
            setSpan(deepLinkAction, linkStartIndex, tipText.length, 0)

            val colorSpan = ForegroundColorSpan(homeViewTipsLabel.currentTextColor)
            setSpan(colorSpan, linkStartIndex, tipText.length, 0)
        }

        homeViewTipsLabel.text = textWithDeepLink
    }

    private fun showFocusSubtitle() {
        keyboardLinearLayout.homeViewTipsLabel.text = getString(R.string.teaser)
        keyboardLinearLayout.homeViewTipsLabel.alpha = 1f
        keyboardLinearLayout.homeViewTipsLabel.setOnClickListener(null)
    }

    private fun adjustViewToStatusBarHeight(statusBarHeight: Int) {
        val inputHeight = resources.getDimension(R.dimen.urlinput_height)
        if (keyboardLinearLayout.layoutParams is ViewGroup.MarginLayoutParams) {
            val marginParams = keyboardLinearLayout.layoutParams as ViewGroup.MarginLayoutParams
            marginParams.topMargin = (inputHeight + statusBarHeight).toInt()
        }

        urlInputLayout.layoutParams.height = (inputHeight + statusBarHeight).toInt()

        if (searchViewContainer.layoutParams is ViewGroup.MarginLayoutParams) {
            val marginParams = searchViewContainer.layoutParams as ViewGroup.MarginLayoutParams
            marginParams.topMargin = (inputHeight + statusBarHeight).toInt()
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_urlinput2, container, false)

        val topSites = view.findViewById<ComposeView>(R.id.topSites)
        topSites.setContent {
            val topSitesState = requireComponents.appStore.observeAsComposableState { state -> state.topSites }

            HomeScreen(
                topSites = topSitesState.value!!,
                topSitesMenuItems = listOfNotNull(
                    TopSiteMenuItem(
                        title = stringResource(R.string.remove_top_site),
                        onClick = { topSite ->
                            viewLifecycleOwner.lifecycleScope.launch(IO) {
                                requireComponents.topSitesUseCases.removeTopSites(topSite)
                            }
                        }
                    )
                ),
                onTopSiteClick = { topSite ->
                    requireComponents.tabsUseCases.addTab(
                        url = topSite.url,
                        source = SessionState.Source.Internal.HomeScreen,
                        selectTab = true,
                        private = true
                    )
                }
            )
        }

        return view
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        toolbarIntegration.set(
            InputToolbarIntegration(
                browserToolbar,
                fragment = this,
                shippedDomainsProvider = shippedDomainsProvider,
                customDomainsProvider = customDomainsProvider
            ),
            owner = this,
            view = browserToolbar
        )

        topSitesFeature.set(
            feature = TopSitesFeature(
                view = DefaultTopSitesView(requireComponents.appStore),
                storage = requireComponents.topSitesStorage,
                config = {
                    TopSitesConfig(
                        totalSites = 4,
                        frecencyConfig = null
                    )
                }
            ),
            owner = this,
            view = view
        )

        dismissView.setOnClickListener(this)

        if (urlInputContainerView != null) {
            OneShotOnPreDrawListener(urlInputContainerView) {
                animateFirstDraw()
                true
            }
        }

        if (isOverlay) {
            keyboardLinearLayout?.visibility = View.GONE
        } else {
            backgroundView?.setBackgroundResource(R.drawable.dark_background)

            dismissView?.visibility = View.GONE

            menuView?.visibility = View.VISIBLE
            menuView?.setOnClickListener(this)
        }

        val isDDG: Boolean =
            requireComponents.store.state.search.selectedOrDefaultSearchEngine?.name == duckDuckGo

        tab?.let { tab ->
            browserToolbar.url =
                if (tab.content.isSearch &&
                    !isDDG &&
                    Features.SEARCH_TERMS_OR_URL
                ) {
                    tab.content.searchTerms
                } else {
                    tab.content.url
                }

            searchViewContainer?.visibility = View.GONE
            menuView?.visibility = View.GONE
        }

        browserToolbar.editMode()

        updateTipsLabel()
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
            if (Settings.getInstance(it.applicationContext).shouldShowFirstrun()) return@onStart
        }
    }

    override fun onStop() {
        super.onStop()

        // Reset the keyboard layout to avoid a jarring animation when the view is started again. (#1135)
        keyboardLinearLayout?.reset()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        if (newConfig.orientation != Configuration.ORIENTATION_UNDEFINED) {
            // This is a hack to make the HomeMenu actually stick on top of the menuView (#3287)

            displayedPopupMenu?.let {
                it.dismiss()

                OneShotOnPreDrawListener(menuView) {
                    showHomeMenu(menuView)
                    false
                }
            }
        }
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @Suppress("ComplexMethod")
    override fun onClick(view: View) {
        when (view.id) {
            R.id.dismissView -> if (isOverlay) {
                animateAndDismiss()
            } else {
                // This is a bit hacky, but emulates what the legacy toolbar did before we replaced
                // it with `browser-toolbar`. First we clear the text and then we invoke the "text
                // changed" callback manually for this change.
                browserToolbar.edit.updateUrl("", false)
                onTextChange("")
            }

            R.id.menuView -> showHomeMenu(view)

            R.id.settings -> {
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.Start)
                )
            }

            R.id.help -> {
                requireComponents.tabsUseCases.addTab(
                    SupportUtils.HELP_URL,
                    source = SessionState.Source.Internal.Menu,
                    private = true
                )
            }

            else -> throw IllegalStateException("Unhandled view in onClick()")
        }
    }

    private fun showHomeMenu(anchor: View) = context?.let {
        displayedPopupMenu = HomeMenu(it, this)

        displayedPopupMenu?.show(anchor)

        displayedPopupMenu?.dismissListener = {
            displayedPopupMenu = null
        }
    }

    override fun onDetach() {
        super.onDetach()

        // On detach, the PopupMenu is no longer relevant to other content (e.g. BrowserFragment) so dismiss it.
        // Note: if we don't dismiss the PopupMenu, its onMenuItemClick method references the old Fragment, which now
        // has a null Context and will cause crashes.
        displayedPopupMenu?.dismiss()
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
        dismissView?.isClickable = false

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
                (urlInputContainerView?.layoutParams as FrameLayout.LayoutParams).bottomMargin
            else
                0
            ).toFloat()

        if (urlInputBackgroundView != null) {
            val width = urlInputBackgroundView.width.toFloat()
            val height = urlInputBackgroundView.height.toFloat()

            val widthScale = if (isOverlay)
                (width + 2 * xyOffset) / width
            else
                1f

            val heightScale = if (isOverlay)
                (height + 2 * xyOffset) / height
            else
                1f

            if (!reverse) {
                urlInputBackgroundView?.pivotX = 0f
                urlInputBackgroundView?.pivotY = 0f
                urlInputBackgroundView?.scaleX = widthScale
                urlInputBackgroundView?.scaleY = heightScale
                urlInputBackgroundView?.translationX = -xyOffset
                urlInputBackgroundView?.translationY = -xyOffset
            }

            // Let the URL input use the full width/height and then shrink to the actual size
            urlInputBackgroundView.animate()
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
        }

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

        if (toolbarBackgroundView != null) {
            val transitionDrawable = toolbarBackgroundView?.background as TransitionDrawable

            if (reverse) {
                transitionDrawable.reverseTransition(ANIMATION_DURATION)
                toolbarBottomBorder?.visibility = View.VISIBLE

                if (!isOverlay) {
                    dismissView?.visibility = View.GONE
                    menuView?.visibility = View.VISIBLE
                }
            } else {
                transitionDrawable.startTransition(ANIMATION_DURATION)
                toolbarBottomBorder?.visibility = View.GONE
            }
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

            ViewUtils.hideKeyboard(browserToolbar)

            if (handleL10NTrigger(input)) return

            val (isUrl, url, searchTerms) = normalizeUrlAndSearchTerms(input)

            openUrl(url, searchTerms)

            TelemetryWrapper.urlBarEvent(isUrl)
        }
    }

    // This isn't that complex, and is only used for l10n screenshots.
    @Suppress("ComplexMethod")
    private fun handleL10NTrigger(input: String): Boolean {
        if (!AppConstants.isDevBuild) return false

        var triggerHandled = true

        when (input) {
            "l10n:tip:1" -> updateSubtitle(Tip.createTrackingProtectionTip(requireContext()))
            "l10n:tip:2" -> updateSubtitle(Tip.createHomescreenTip(requireContext()))
            "l10n:tip:3" -> updateSubtitle(Tip.createDefaultBrowserTip(requireContext()))
            "l10n:tip:4" -> updateSubtitle(Tip.createAutocompleteURLTip(requireContext()))
            "l10n:tip:5" -> updateSubtitle(Tip.createOpenInNewTabTip(requireContext()))
            "l10n:tip:6" -> updateSubtitle(Tip.createRequestDesktopTip(requireContext()))
            "l10n:tip:7" -> updateSubtitle(Tip.createAllowlistTip(requireContext()))
            else -> triggerHandled = false
        }

        if (triggerHandled) {
            browserToolbar.displayMode()
            browserToolbar.editMode()
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
    }

    private fun openUrl(url: String, searchTerms: String?) {
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

    internal fun onTextChange(text: String) {
        searchSuggestionsViewModel.setSearchQuery(text)

        if (text.trim { it <= ' ' }.isEmpty()) {
            searchViewContainer?.visibility = View.GONE

            if (!isOverlay) {
                playVisibilityAnimation(true)
            }
        } else {
            menuView?.visibility = View.GONE

            if (!isOverlay && dismissView?.visibility != View.VISIBLE) {
                playVisibilityAnimation(false)
                dismissView?.visibility = View.VISIBLE
            }

            searchViewContainer?.visibility = View.VISIBLE
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        if (key == activity?.getString(R.string.pref_key_homescreen_tips)) { updateTipsLabel() }
    }
}
