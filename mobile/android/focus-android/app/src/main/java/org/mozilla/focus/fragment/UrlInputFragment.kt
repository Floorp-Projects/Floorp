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
import android.text.InputType
import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.URLUtil
import android.widget.FrameLayout
import android.widget.TextView
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.preference.PreferenceManager
import kotlinx.android.synthetic.main.fragment_urlinput.*
import kotlinx.android.synthetic.main.fragment_urlinput.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.CustomDomains
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteResult
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.SessionState
import mozilla.components.support.utils.ThreadUtils
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText.AutocompleteResult
import org.mozilla.focus.R
import org.mozilla.focus.ext.contentState
import org.mozilla.focus.ext.isSearch
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.locale.LocaleAwareFragment
import org.mozilla.focus.menu.home.HomeMenu
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.ui.SearchSuggestionsFragment
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.tips.Tip
import org.mozilla.focus.tips.TipManager
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.Features
import org.mozilla.focus.utils.OneShotOnPreDrawListener
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.StatusBarUtils
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import org.mozilla.focus.whatsnew.WhatsNew
import java.util.Objects
import kotlin.coroutines.CoroutineContext

class FocusCrashException : Exception()

/**
 * Fragment for displaying the URL input controls.
 */
// Refactoring the size and function count of this fragment is non-trivial at this point.
// Therefore we ignore those violations for now.
@Suppress("LargeClass", "TooManyFunctions")
class UrlInputFragment :
    LocaleAwareFragment(),
    View.OnClickListener,
    SharedPreferences.OnSharedPreferenceChangeListener,
    CoroutineScope {
    companion object {
        @JvmField
        val FRAGMENT_TAG = "url_input"

        private const val duckDuckGo = "DuckDuckGo"

        private val ARGUMENT_ANIMATION = "animation"
        private val ARGUMENT_X = "x"
        private val ARGUMENT_Y = "y"
        private val ARGUMENT_WIDTH = "width"
        private val ARGUMENT_HEIGHT = "height"

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
        fun createWithTab(tabId: String, urlView: View): UrlInputFragment {
            val arguments = Bundle()

            arguments.putString(ARGUMENT_SESSION_UUID, tabId)
            arguments.putString(ARGUMENT_ANIMATION, ANIMATION_BROWSER_SCREEN)

            val screenLocation = IntArray(2)
            urlView.getLocationOnScreen(screenLocation)

            arguments.putInt(ARGUMENT_X, screenLocation[0])
            arguments.putInt(ARGUMENT_Y, screenLocation[1])
            arguments.putInt(ARGUMENT_WIDTH, urlView.width)
            arguments.putInt(ARGUMENT_HEIGHT, urlView.height)

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
    private var useShipped = false
    private var useCustom = false
    private var displayedPopupMenu: HomeMenu? = null

    @Volatile
    private var isAnimating: Boolean = false

    private var session: Session? = null

    private val isOverlay: Boolean
        get() = session != null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProviders.of(this).get(SearchSuggestionsViewModel::class.java)

        PreferenceManager.getDefaultSharedPreferences(context)
            .registerOnSharedPreferenceChangeListener(this)

        // Get session from session manager if there's a session UUID in the fragment's arguments
        arguments?.getString(ARGUMENT_SESSION_UUID)?.let { id ->
            session = requireComponents.sessionManager.findSessionById(id)
        }
    }

    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        childFragmentManager.beginTransaction()
                .replace(searchViewContainer.id, SearchSuggestionsFragment.create())
                .commit()

        searchSuggestionsViewModel.selectedSearchSuggestion.observe(this, Observer {
            val isSuggestion = searchSuggestionsViewModel.searchQuery.value != it
            it?.let {
                if (searchSuggestionsViewModel.alwaysSearch) {
                    onSearch(it, false, true)
                } else {
                    onSearch(it, isSuggestion)
                }
            }
        })
    }

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        activity?.let {
            val settings = Settings.getInstance(it.applicationContext)
            useCustom = settings.shouldAutocompleteFromCustomDomainList()
            useShipped = settings.shouldAutocompleteFromShippedDomainList()
            shippedDomainsProvider.initialize(it.applicationContext)
            customDomainsProvider.initialize(it.applicationContext)
        }

        StatusBarUtils.getStatusBarHeight(keyboardLinearLayout) {
            adjustViewToStatusBarHeight(it)
        }
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
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

        if (addToAutoComplete.layoutParams is ViewGroup.MarginLayoutParams) {
            val marginParams = addToAutoComplete.layoutParams as ViewGroup.MarginLayoutParams
            marginParams.topMargin = (inputHeight + statusBarHeight).toInt()
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?):
        View? = inflater.inflate(R.layout.fragment_urlinput, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        listOf(dismissView, clearView).forEach { it.setOnClickListener(this) }

        urlView?.setOnFilterListener(::onFilter)
        urlView?.setOnTextChangeListener(::onTextChange)
        urlView?.imeOptions = urlView.imeOptions or ViewUtils.IME_FLAG_NO_PERSONALIZED_LEARNING
        urlView?.inputType = urlView.inputType or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS

        if (urlInputContainerView != null) {
            OneShotOnPreDrawListener(urlInputContainerView) {
                animateFirstDraw()
                true
            }
        }

        if (isOverlay) {
            keyboardLinearLayout?.visibility = View.GONE
        } else {
            backgroundView?.setBackgroundResource(R.drawable.background_gradient)

            dismissView?.visibility = View.GONE

            menuView?.visibility = View.VISIBLE
            menuView?.setOnClickListener(this)
        }

        urlView?.setOnCommitListener(::onCommit)

        val isDDG: Boolean =
            Settings.getInstance(requireContext()).defaultSearchEngineName == duckDuckGo

        session?.let {
            val contentState = requireComponents.store.contentState(it.id)
            urlView?.setText(
                if (contentState?.isSearch == true &&
                    !isDDG &&
                    Features.SEARCH_TERMS_OR_URL
                ) {
                    contentState.searchTerms
                } else {
                    it.url
                }
            )

            clearView?.visibility = View.VISIBLE
            searchViewContainer?.visibility = View.GONE
            addToAutoComplete?.visibility = View.VISIBLE
        }

        addToAutoComplete.setOnClickListener {
            val url = urlView?.text.toString()
            addUrlToAutocomplete(url)
        }

        updateTipsLabel()
    }

    override fun applyLocale() {
        if (isOverlay) {
            activity?.supportFragmentManager
                    ?.beginTransaction()
                    ?.replace(
                            R.id.container,
                            UrlInputFragment.createWithTab(session!!.id, urlView),
                            UrlInputFragment.FRAGMENT_TAG)
                    ?.commit()
        } else {
            activity?.supportFragmentManager
                    ?.beginTransaction()
                    ?.replace(
                            R.id.container,
                            UrlInputFragment.createWithBackground(),
                            UrlInputFragment.FRAGMENT_TAG)
                    ?.commit()
        }
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
            if (Settings.getInstance(it.applicationContext).shouldShowFirstrun()) return@onStart }
        showKeyboard()
    }

    override fun onStop() {
        super.onStop()

        // Reset the keyboard layout to avoid a jarring animation when the view is started again. (#1135)
        keyboardLinearLayout?.reset()
    }

    fun showKeyboard() {
        ViewUtils.showKeyboard(urlView)
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
            R.id.clearView -> clear()

            R.id.dismissView -> if (isOverlay) {
                animateAndDismiss()
            } else {
                clear()
            }

            R.id.menuView -> showHomeMenu(view)

            R.id.whats_new -> context?.let {
                TelemetryWrapper.openWhatsNewEvent(WhatsNew.shouldHighlightWhatsNew(it))

                WhatsNew.userViewedWhatsNew(it)

                val url = SupportUtils.getSumoURLForTopic(it, SupportUtils.SumoTopic.WHATS_NEW)
                requireComponents.tabsUseCases.addPrivateTab(url, source = SessionState.Source.MENU)
            }

            R.id.settings -> (activity as LocaleAwareAppCompatActivity).openPreferences()

            R.id.help -> {
                requireComponents.tabsUseCases.addPrivateTab(
                    SupportUtils.HELP_URL,
                    source = SessionState.Source.MENU
                )
            }

            else -> throw IllegalStateException("Unhandled view in onClick()")
        }
    }

    private fun clear() {
        urlView?.setText("")
        urlView?.requestFocus()
    }

    private fun showHomeMenu(anchor: View) = context?.let {
        displayedPopupMenu = HomeMenu(it, this)

        displayedPopupMenu?.show(anchor)

        displayedPopupMenu?.dismissListener = {
            displayedPopupMenu = null
        }
    }

    private fun addUrlToAutocomplete(url: String) {
        var duplicateURL = false
        val job = launch(IO) {
            duplicateURL = CustomDomains.load(requireContext()).contains(url)

            if (duplicateURL) return@launch
            CustomDomains.add(requireContext(), url)

            TelemetryWrapper.saveAutocompleteDomainEvent(TelemetryWrapper.AutoCompleteEventSource.QUICK_ADD)
        }

        job.invokeOnCompletion {
            requireActivity().runOnUiThread {
                val messageId =
                        if (!duplicateURL)
                            R.string.preference_autocomplete_add_confirmation
                        else
                            R.string.preference_autocomplete_duplicate_url_error

                ViewUtils.showBrandedSnackbar(Objects.requireNonNull<View>(view), messageId, 0)
                animateAndDismiss()
            }
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

        val xyOffset = (if (isOverlay)
            (urlInputContainerView?.layoutParams as FrameLayout.LayoutParams).bottomMargin
        else
            0).toFloat()

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

                clearView?.alpha = 0f
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
                    override fun onAnimationStart(animation: Animator) {
                        if (reverse) {
                            clearView?.alpha = 0f
                        }
                    }

                    override fun onAnimationEnd(animation: Animator) {
                        if (reverse) {
                            if (isOverlay) {
                                dismiss()
                            }
                        } else {
                            clearView?.alpha = 1f
                        }

                        isAnimating = false
                    }
                })
        }

        // We only need to animate the toolbar if we are an overlay.
        if (isOverlay) {
            val screenLocation = IntArray(2)
            urlView?.getLocationOnScreen(screenLocation)

            val leftDelta = arguments!!.getInt(ARGUMENT_X) - screenLocation[0] - urlView.paddingLeft

            if (!reverse) {
                urlView?.pivotX = 0f
                urlView?.pivotY = 0f
                urlView?.translationX = leftDelta.toFloat()
            }

            if (urlView != null) {
                // The URL moves from the right (at least if the lock is visible) to it's actual position
                urlView.animate()
                    .setDuration(ANIMATION_DURATION.toLong())
                    .translationX((if (reverse) leftDelta else 0).toFloat())
            }
        }

        if (!reverse) {
            clearView?.alpha = 0f
        }

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
        activity?.supportFragmentManager
            ?.beginTransaction()
            ?.remove(this)
            ?.commitAllowingStateLoss()
    }

    private fun onCommit() {
        val input = if (urlView.autocompleteResult == null) {
            urlView?.text.toString()
        } else {
            urlView.autocompleteResult!!.text.let {
                if (it.isEmpty() || !URLUtil.isValidUrl(urlView?.text.toString())) {
                    urlView?.text.toString()
                } else it
            }
        }

        if (input.trim { it <= ' ' }.isNotEmpty()) {
            handleCrashTrigger(input)

            ViewUtils.hideKeyboard(urlView)

            if (handleL10NTrigger(input)) return

            val (isUrl, url, searchTerms) = normalizeUrlAndSearchTerms(input)

            openUrl(url, searchTerms)

            if (urlView.autocompleteResult != null) {
                TelemetryWrapper.urlBarEvent(isUrl, urlView.autocompleteResult!!)
            }
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

        if (triggerHandled) clear()
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
            UrlUtils.createSearchUrl(context, input)

        val searchTerms = if (isUrl)
            null
        else
            input.trim { it <= ' ' }
        return Triple(isUrl, url, searchTerms)
    }

    private fun onSearch(query: String, isSuggestion: Boolean = false, alwaysSearch: Boolean = false) {
        if (alwaysSearch) {
            openUrl(UrlUtils.createSearchUrl(context, query), query)
        } else {
            val searchTerms = if (UrlUtils.isUrl(query)) null else query
            val searchUrl =
                if (searchTerms != null) UrlUtils.createSearchUrl(context, searchTerms) else UrlUtils.normalize(query)

            openUrl(searchUrl, searchTerms)
        }

        TelemetryWrapper.searchSelectEvent(isSuggestion)
    }

    private fun openUrl(url: String, searchTerms: String?) {
        if (!searchTerms.isNullOrEmpty()) {
            session?.let {
                requireComponents.store.dispatch(ContentAction.UpdateSearchTermsAction(it.id, searchTerms))
            }
        }

        val fragmentManager = requireActivity().supportFragmentManager

        if (session != null) {
            requireComponents.sessionUseCases.loadUrl(url, session?.id)

            fragmentManager.beginTransaction()
                .remove(this)
                .commit()
        } else {
            val tabId = requireComponents.tabsUseCases.addPrivateTab(
                url,
                source = SessionState.Source.USER_ENTERED,
                selectTab = true
            )

            if (!searchTerms.isNullOrEmpty()) {
                requireComponents.store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, searchTerms))
            }
        }
    }

    private fun onFilter(searchText: String) {
        onFilter(searchText, urlView)
    }

    @Suppress("ComplexMethod")
    private fun onFilter(searchText: String, view: InlineAutocompleteEditText?) {
        // If the UrlInputFragment has already been hidden, don't bother with filtering. Because of the text
        // input architecture on Android it's possible for onFilter() to be called after we've already
        // hidden the Fragment, see the relevant bug for more background:
        // https://github.com/mozilla-mobile/focus-android/issues/441#issuecomment-293691141
        if (!isVisible) {
            return
        }

        view?.let {
            var result: DomainAutocompleteResult? = null
            if (useCustom) {
                result = customDomainsProvider.getAutocompleteSuggestion(searchText)
            }

            if (useShipped && result == null) {
                result = shippedDomainsProvider.getAutocompleteSuggestion(searchText)
            }

            if (result != null) {
                view.applyAutocompleteResult(AutocompleteResult(
                        result.text, result.source, result.totalItems, { result.url }))
            } else {
                view.noAutocompleteResult()
            }
        }
    }

    private fun onTextChange(text: String, @Suppress("UNUSED_PARAMETER") autocompleteText: String) {
        searchSuggestionsViewModel.setSearchQuery(text)

        if (text.trim { it <= ' ' }.isEmpty()) {
            clearView?.visibility = View.GONE
            searchViewContainer?.visibility = View.GONE
            addToAutoComplete?.visibility = View.GONE

            if (!isOverlay) {
                playVisibilityAnimation(true)
            }
        } else {
            clearView?.visibility = View.VISIBLE
            menuView?.visibility = View.GONE

            if (!isOverlay && dismissView?.visibility != View.VISIBLE) {
                playVisibilityAnimation(false)
                dismissView?.visibility = View.VISIBLE
            }

            searchViewContainer?.visibility = View.VISIBLE
            addToAutoComplete?.visibility = View.GONE
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        if (key == activity?.getString(R.string.pref_key_homescreen_tips)) { updateTipsLabel() }
    }
}
