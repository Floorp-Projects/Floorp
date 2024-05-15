/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.os.StrictMode
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.fragment.app.setFragmentResultListener
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.utils.ext.isLandscape
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.GleanMetrics.AddressToolbar
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.IncompleteRedesignToolbarFeature
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.messaging.FenixMessageSurfaceId
import org.mozilla.fenix.messaging.MessagingFeature
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.getCookieBannerUIMode
import org.mozilla.fenix.shopping.DefaultShoppingExperienceFeature
import org.mozilla.fenix.shopping.ReviewQualityCheckFeature
import org.mozilla.fenix.shortcut.PwaOnboardingObserver
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.translations.TranslationsDialogFragment.Companion.SESSION_ID
import org.mozilla.fenix.translations.TranslationsDialogFragment.Companion.TRANSLATION_IN_PROGRESS

/**
 * Fragment used for browsing the web within the main app.
 */
@Suppress("TooManyFunctions", "LargeClass")
class BrowserFragment : BaseBrowserFragment(), UserInteractionHandler {
    private val windowFeature = ViewBoundFeatureWrapper<WindowFeature>()
    private val openInAppOnboardingObserver = ViewBoundFeatureWrapper<OpenInAppOnboardingObserver>()
    private val standardSnackbarErrorBinding =
        ViewBoundFeatureWrapper<StandardSnackbarErrorBinding>()
    private val reviewQualityCheckFeature = ViewBoundFeatureWrapper<ReviewQualityCheckFeature>()
    private val translationsBinding = ViewBoundFeatureWrapper<TranslationsBinding>()

    @VisibleForTesting
    internal val messagingFeature = ViewBoundFeatureWrapper<MessagingFeature>()

    private var readerModeAvailable = false
    private var reviewQualityCheckAvailable = false
    private var translationsAvailable = false

    private var pwaOnboardingObserver: PwaOnboardingObserver? = null

    @VisibleForTesting
    internal var leadingAction: BrowserToolbar.Button? = null
    private var forwardAction: BrowserToolbar.TwoStateButton? = null
    private var backAction: BrowserToolbar.TwoStateButton? = null
    private var refreshAction: BrowserToolbar.TwoStateButton? = null
    private var isTablet: Boolean = false

    @Suppress("LongMethod")
    override fun initializeUI(view: View, tab: SessionState) {
        super.initializeUI(view, tab)

        val context = requireContext()
        val components = context.components

        if (!context.isTabStripEnabled() && context.settings().isSwipeToolbarToSwitchTabsEnabled) {
            binding.gestureLayout.addGestureListener(
                ToolbarGestureHandler(
                    activity = requireActivity(),
                    contentLayout = binding.browserLayout,
                    tabPreview = binding.tabPreview,
                    toolbarLayout = browserToolbarView.view,
                    store = components.core.store,
                    selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                    onSwipeStarted = {
                        thumbnailsFeature.get()?.requestScreenshot()
                    },
                ),
            )
        }

        updateBrowserToolbarLeadingAndNavigationActions(
            context = context,
            redesignEnabled = IncompleteRedesignToolbarFeature(context.settings()).isEnabled,
            isLandscape = context.isLandscape(),
            isTablet = resources.getBoolean(R.bool.tablet),
            isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate,
            feltPrivateBrowsingEnabled = context.settings().feltPrivateBrowsingEnabled,
        )

        val readerModeAction =
            BrowserToolbar.ToggleButton(
                image = AppCompatResources.getDrawable(
                    context,
                    R.drawable.ic_readermode,
                )!!,
                imageSelected =
                AppCompatResources.getDrawable(
                    context,
                    R.drawable.ic_readermode_selected,
                )!!,
                contentDescription = context.getString(R.string.browser_menu_read),
                contentDescriptionSelected = context.getString(R.string.browser_menu_read_close),
                visible = {
                    readerModeAvailable && !reviewQualityCheckAvailable
                },
                weight = { READER_MODE_WEIGHT },
                selected = getSafeCurrentTab()?.let {
                    activity?.components?.core?.store?.state?.findTab(it.id)?.readerState?.active
                } ?: false,
                listener = browserToolbarInteractor::onReaderModePressed,
            )

        browserToolbarView.view.addPageAction(readerModeAction)

        initTranslationsAction(context, view)
        initReviewQualityCheck(context, view)
        initSharePageAction(context)
        initReloadAction(context)

        thumbnailsFeature.set(
            feature = BrowserThumbnails(context, binding.engineView, components.core.store),
            owner = this,
            view = view,
        )

        readerViewFeature.set(
            feature = components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                ReaderViewFeature(
                    context,
                    components.core.engine,
                    components.core.store,
                    binding.readerViewControlsBar,
                ) { available, active ->
                    if (available) {
                        ReaderMode.available.record(NoExtras())
                    }

                    readerModeAvailable = available
                    readerModeAction.setSelected(active)
                    safeInvalidateBrowserToolbarView()
                }
            },
            owner = this,
            view = view,
        )

        windowFeature.set(
            feature = WindowFeature(
                store = components.core.store,
                tabsUseCases = components.useCases.tabsUseCases,
            ),
            owner = this,
            view = view,
        )

        if (context.settings().shouldShowOpenInAppCfr) {
            openInAppOnboardingObserver.set(
                feature = OpenInAppOnboardingObserver(
                    context = context,
                    store = context.components.core.store,
                    lifecycleOwner = this,
                    navController = findNavController(),
                    settings = context.settings(),
                    appLinksUseCases = context.components.useCases.appLinksUseCases,
                    container = binding.browserLayout as ViewGroup,
                    shouldScrollWithTopToolbar = !context.settings().shouldUseBottomToolbar,
                ),
                owner = this,
                view = view,
            )
        }

        standardSnackbarErrorBinding.set(
            feature = StandardSnackbarErrorBinding(
                requireActivity(),
                requireActivity().components.appStore,
            ),
            owner = viewLifecycleOwner,
            view = binding.root,
        )

        setTranslationFragmentResultListener()

        setupMicrosurvey()
    }

    @VisibleForTesting
    internal fun setupMicrosurvey(isMicrosurveyEnabled: Boolean = FeatureFlags.microsurveysEnabled) {
        if (requireContext().settings().isExperimentationEnabled && isMicrosurveyEnabled) {
            messagingFeature.set(
                feature = MessagingFeature(
                    appStore = requireComponents.appStore,
                    surface = FenixMessageSurfaceId.MICROSURVEY,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }
    }

    private fun setTranslationFragmentResultListener() {
        setFragmentResultListener(
            TRANSLATION_IN_PROGRESS,
        ) { _, result ->
            result.getString(SESSION_ID)?.let {
                if (it == getCurrentTab()?.id) {
                    FenixSnackbar.make(
                        view = binding.dynamicSnackbarContainer,
                        duration = Snackbar.LENGTH_LONG,
                        isDisplayedWithBrowserToolbar = true,
                    )
                        .setText(requireContext().getString(R.string.translation_in_progress_snackbar))
                        .show()
                }
            }
        }
    }

    private fun initSharePageAction(context: Context) {
        if (!IncompleteRedesignToolbarFeature(context.settings()).isEnabled) {
            return
        }

        val sharePageAction =
            BrowserToolbar.Button(
                imageDrawable = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_share_android_24,
                )!!,
                contentDescription = getString(R.string.browser_menu_share),
                weight = { SHARE_WEIGHT },
                iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                listener = {
                    AddressToolbar.shareTapped.record((NoExtras()))
                    browserToolbarInteractor.onShareActionClicked()
                },
            )

        browserToolbarView.view.addPageAction(sharePageAction)
    }

    private fun initTranslationsAction(context: Context, view: View) {
        if (
            !FxNimbus.features.translations.value().mainFlowToolbarEnabled
        ) {
            return
        }

        val translationsAction = Toolbar.ActionButton(
            AppCompatResources.getDrawable(
                context,
                R.drawable.mozac_ic_translate_24,
            ),
            contentDescription = context.getString(R.string.browser_toolbar_translate),
            iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            visible = { translationsAvailable },
            weight = { TRANSLATIONS_WEIGHT },
            listener = {
                browserToolbarInteractor.onTranslationsButtonClicked()
            },
        )
        browserToolbarView.view.addPageAction(translationsAction)

        translationsBinding.set(
            feature = TranslationsBinding(
                browserStore = context.components.core.store,
                onTranslationsActionUpdated = {
                    translationsAvailable = it.isVisible

                    translationsAction.updateView(
                        tintColorResource = if (it.isTranslated) {
                            R.color.fx_mobile_icon_color_accent_violet
                        } else {
                            ThemeManager.resolveAttribute(R.attr.textPrimary, context)
                        },
                        contentDescription = if (it.isTranslated) {
                            context.getString(
                                R.string.browser_toolbar_translated_successfully,
                                it.fromSelectedLanguage?.localizedDisplayName,
                                it.toSelectedLanguage?.localizedDisplayName,
                            )
                        } else {
                            context.getString(R.string.browser_toolbar_translate)
                        },
                    )

                    safeInvalidateBrowserToolbarView()
                },
                onShowTranslationsDialog = {
                    browserToolbarInteractor.onTranslationsButtonClicked()
                },
            ),
            owner = this,
            view = view,
        )
    }

    private fun initReloadAction(context: Context) {
        if (!IncompleteRedesignToolbarFeature(context.settings()).isEnabled) {
            return
        }

        refreshAction =
            BrowserToolbar.TwoStateButton(
                primaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_arrow_clockwise_24,
                )!!,
                primaryContentDescription = context.getString(R.string.browser_menu_refresh),
                primaryImageTintResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                isInPrimaryState = {
                    getSafeCurrentTab()?.content?.loading == false
                },
                secondaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_stop,
                )!!,
                secondaryContentDescription = context.getString(R.string.browser_menu_stop),
                disableInSecondaryState = false,
                weight = { RELOAD_WEIGHT },
                longClickListener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Reload(bypassCache = true),
                    )
                },
                listener = {
                    if (getCurrentTab()?.content?.loading == true) {
                        AddressToolbar.cancelTapped.record((NoExtras()))
                        browserToolbarInteractor.onBrowserToolbarMenuItemTapped(ToolbarMenu.Item.Stop)
                    } else {
                        AddressToolbar.reloadTapped.record((NoExtras()))
                        browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                            ToolbarMenu.Item.Reload(bypassCache = false),
                        )
                    }
                },
            )

        refreshAction?.let {
            browserToolbarView.view.addPageAction(it)
        }
    }

    private fun initReviewQualityCheck(context: Context, view: View) {
        val reviewQualityCheck =
            BrowserToolbar.ToggleButton(
                image = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_shopping_24,
                )!!.apply {
                    setTint(ContextCompat.getColor(context, R.color.fx_mobile_text_color_primary))
                },
                imageSelected = AppCompatResources.getDrawable(
                    context,
                    R.drawable.ic_shopping_selected,
                )!!,
                contentDescription = context.getString(R.string.review_quality_check_open_handle_content_description),
                contentDescriptionSelected =
                context.getString(R.string.review_quality_check_close_handle_content_description),
                visible = { reviewQualityCheckAvailable },
                weight = { REVIEW_QUALITY_CHECK_WEIGHT },
                listener = { _ ->
                    requireComponents.appStore.dispatch(
                        AppAction.ShoppingAction.ShoppingSheetStateUpdated(expanded = true),
                    )

                    findNavController().navigate(
                        BrowserFragmentDirections.actionBrowserFragmentToReviewQualityCheckDialogFragment(),
                    )
                    Shopping.addressBarIconClicked.record()
                },
            )

        browserToolbarView.view.addPageAction(reviewQualityCheck)

        reviewQualityCheckFeature.set(
            feature = ReviewQualityCheckFeature(
                appStore = requireComponents.appStore,
                browserStore = context.components.core.store,
                shoppingExperienceFeature = DefaultShoppingExperienceFeature(),
                onIconVisibilityChange = {
                    if (!reviewQualityCheckAvailable && it) {
                        Shopping.addressBarIconDisplayed.record()
                    }
                    reviewQualityCheckAvailable = it
                    safeInvalidateBrowserToolbarView()
                },
                onBottomSheetStateChange = {
                    reviewQualityCheck.setSelected(selected = it, notifyListener = false)
                },
                onProductPageDetected = {
                    Shopping.productPageVisits.add()
                },
            ),
            owner = this,
            view = view,
        )
    }

    // Adds a home button to BrowserToolbar or, if FeltPrivateBrowsing is enabled, a clear data button instead.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun addLeadingAction(
        context: Context,
        feltPrivateBrowsingEnabled: Boolean,
        isPrivate: Boolean,
    ) {
        if (leadingAction == null) {
            leadingAction = if (isPrivate && feltPrivateBrowsingEnabled) {
                BrowserToolbar.Button(
                    imageDrawable = AppCompatResources.getDrawable(
                        context,
                        R.drawable.mozac_ic_data_clearance_24,
                    )!!,
                    contentDescription = context.getString(R.string.browser_toolbar_erase),
                    iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                    listener = browserToolbarInteractor::onEraseButtonClicked,
                )
            } else {
                BrowserToolbar.Button(
                    imageDrawable = AppCompatResources.getDrawable(
                        context,
                        R.drawable.mozac_ic_home_24,
                    )!!,
                    contentDescription = context.getString(R.string.browser_toolbar_home),
                    iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                    listener = browserToolbarInteractor::onHomeButtonClicked,
                )
            }.also {
                browserToolbarView.view.addNavigationAction(it)
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun removeLeadingAction() {
        leadingAction?.let {
            browserToolbarView.view.removeNavigationAction(it)
        }
        leadingAction = null
    }

    /**
     * This code takes care of the [BrowserToolbar] leading and navigation actions.
     * The older design requires a HomeButton followed by navigation buttons for tablets.
     * The newer design expects NavigationButtons and a HomeButton in landscape mode for phones and in both modes
     * for tablets.
     */
    @VisibleForTesting
    internal fun updateBrowserToolbarLeadingAndNavigationActions(
        context: Context,
        redesignEnabled: Boolean,
        isLandscape: Boolean,
        isTablet: Boolean,
        isPrivate: Boolean,
        feltPrivateBrowsingEnabled: Boolean,
    ) {
        if (redesignEnabled) {
            updateAddressBarNavigationActions(
                isLandscape = isLandscape,
                isTablet = isTablet,
                context = context,
            )
            updateAddressBarLeadingAction(
                redesignEnabled = true,
                isLandscape = isLandscape,
                isTablet = isTablet,
                isPrivate = isPrivate,
                feltPrivateBrowsingEnabled = feltPrivateBrowsingEnabled,
                context = context,
            )
        } else {
            updateAddressBarLeadingAction(
                redesignEnabled = false,
                isLandscape = isLandscape,
                isPrivate = isPrivate,
                isTablet = isTablet,
                feltPrivateBrowsingEnabled = feltPrivateBrowsingEnabled,
                context = context,
            )
            updateTabletToolbarActions(isTablet = isTablet)
        }
        browserToolbarView.view.invalidateActions()
    }

    @VisibleForTesting
    internal fun updateAddressBarLeadingAction(
        redesignEnabled: Boolean,
        isLandscape: Boolean,
        isTablet: Boolean,
        isPrivate: Boolean,
        feltPrivateBrowsingEnabled: Boolean,
        context: Context,
    ) {
        if (!redesignEnabled || isLandscape || isTablet) {
            addLeadingAction(
                isPrivate = isPrivate,
                feltPrivateBrowsingEnabled = feltPrivateBrowsingEnabled,
                context = context,
            )
        } else {
            removeLeadingAction()
        }
    }

    @VisibleForTesting
    internal fun updateAddressBarNavigationActions(
        context: Context,
        isLandscape: Boolean,
        isTablet: Boolean,
    ) {
        if (isLandscape || isTablet) {
            addNavigationActions(context)
        } else {
            removeNavigationActions()
        }
    }

    override fun onUpdateToolbarForConfigurationChange(toolbar: BrowserToolbarView) {
        super.onUpdateToolbarForConfigurationChange(toolbar)

        updateBrowserToolbarLeadingAndNavigationActions(
            context = requireContext(),
            redesignEnabled = IncompleteRedesignToolbarFeature(requireContext().settings()).isEnabled,
            isLandscape = requireContext().isLandscape(),
            isTablet = resources.getBoolean(R.bool.tablet),
            isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate,
            feltPrivateBrowsingEnabled = requireContext().settings().feltPrivateBrowsingEnabled,
        )
    }

    @VisibleForTesting
    internal fun updateTabletToolbarActions(isTablet: Boolean) {
        if (isTablet == this.isTablet) return

        if (isTablet) {
            addTabletActions(requireContext())
        } else {
            removeTabletActions()
        }

        this.isTablet = isTablet
    }

    @VisibleForTesting
    internal fun addNavigationActions(context: Context) {
        val enableTint = ThemeManager.resolveAttribute(R.attr.textPrimary, context)
        val disableTint = ThemeManager.resolveAttribute(R.attr.textDisabled, context)

        if (backAction == null) {
            backAction = BrowserToolbar.TwoStateButton(
                primaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_back_24,
                )!!,
                primaryContentDescription = context.getString(R.string.browser_menu_back),
                primaryImageTintResource = enableTint,
                isInPrimaryState = { getSafeCurrentTab()?.content?.canGoBack ?: false },
                secondaryImageTintResource = disableTint,
                disableInSecondaryState = true,
                longClickListener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Back(viewHistory = true),
                    )
                },
                listener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Back(viewHistory = false),
                    )
                },
            ).also {
                browserToolbarView.view.addNavigationAction(it)
            }
        }

        if (forwardAction == null) {
            forwardAction = BrowserToolbar.TwoStateButton(
                primaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_forward_24,
                )!!,
                primaryContentDescription = context.getString(R.string.browser_menu_forward),
                primaryImageTintResource = enableTint,
                isInPrimaryState = { getSafeCurrentTab()?.content?.canGoForward ?: false },
                secondaryImageTintResource = disableTint,
                disableInSecondaryState = true,
                longClickListener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Forward(viewHistory = true),
                    )
                },
                listener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Forward(viewHistory = false),
                    )
                },
            ).also {
                browserToolbarView.view.addNavigationAction(it)
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun addTabletActions(context: Context) {
        addNavigationActions(context)

        val enableTint = ThemeManager.resolveAttribute(R.attr.textPrimary, context)
        if (refreshAction == null) {
            refreshAction = BrowserToolbar.TwoStateButton(
                primaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_arrow_clockwise_24,
                )!!,
                primaryContentDescription = context.getString(R.string.browser_menu_refresh),
                primaryImageTintResource = enableTint,
                isInPrimaryState = {
                    getSafeCurrentTab()?.content?.loading == false
                },
                secondaryImage = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_stop,
                )!!,
                secondaryContentDescription = context.getString(R.string.browser_menu_stop),
                disableInSecondaryState = false,
                longClickListener = {
                    browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Reload(bypassCache = true),
                    )
                },
                listener = {
                    if (getCurrentTab()?.content?.loading == true) {
                        browserToolbarInteractor.onBrowserToolbarMenuItemTapped(ToolbarMenu.Item.Stop)
                    } else {
                        browserToolbarInteractor.onBrowserToolbarMenuItemTapped(
                            ToolbarMenu.Item.Reload(bypassCache = false),
                        )
                    }
                },
            ).also {
                browserToolbarView.view.addNavigationAction(it)
            }
        }
    }

    @VisibleForTesting
    internal fun removeNavigationActions() {
        forwardAction?.let {
            browserToolbarView.view.removeNavigationAction(it)
        }
        forwardAction = null
        backAction?.let {
            browserToolbarView.view.removeNavigationAction(it)
        }
        backAction = null
    }

    @VisibleForTesting
    internal fun removeTabletActions() {
        removeNavigationActions()

        refreshAction?.let {
            browserToolbarView.view.removeNavigationAction(it)
        }
    }

    override fun onStart() {
        super.onStart()
        val context = requireContext()
        val settings = context.settings()

        if (!settings.userKnowsAboutPwas) {
            pwaOnboardingObserver = PwaOnboardingObserver(
                store = context.components.core.store,
                lifecycleOwner = this,
                navController = findNavController(),
                settings = settings,
                webAppUseCases = context.components.useCases.webAppUseCases,
            ).also {
                it.start()
            }
        }

        subscribeToTabCollections()
        updateLastBrowseActivity()
    }

    override fun onStop() {
        super.onStop()
        updateLastBrowseActivity()
        updateHistoryMetadata()
        pwaOnboardingObserver?.stop()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        isTablet = false
        leadingAction = null
        forwardAction = null
        backAction = null
        refreshAction = null
    }

    private fun updateHistoryMetadata() {
        getCurrentTab()?.let { tab ->
            (tab as? TabSessionState)?.historyMetadata?.let {
                requireComponents.core.historyMetadataService.updateMetadata(it, tab)
            }
        }
    }

    private fun subscribeToTabCollections() {
        Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections()
                .observe(viewLifecycleOwner, observer)
        }
    }

    override fun onResume() {
        super.onResume()
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    override fun onBackPressed(): Boolean {
        return readerViewFeature.onBackPressed() || super.onBackPressed()
    }

    override fun navToQuickSettingsSheet(tab: SessionState, sitePermissions: SitePermissions?) {
        val useCase = requireComponents.useCases.trackingProtectionUseCases
        FxNimbus.features.cookieBanners.recordExposure()
        useCase.containsException(tab.id) { hasTrackingProtectionException ->
            lifecycleScope.launch {
                val cookieBannersStorage = requireComponents.core.cookieBannersStorage
                val cookieBannerUIMode = cookieBannersStorage.getCookieBannerUIMode(
                    requireContext(),
                    tab,
                )
                withContext(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        val isTrackingProtectionEnabled =
                            tab.trackingProtection.enabled && !hasTrackingProtectionException
                        val directions =
                            BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                                sessionId = tab.id,
                                url = tab.content.url,
                                title = tab.content.title,
                                isSecured = tab.content.securityInfo.secure,
                                sitePermissions = sitePermissions,
                                gravity = getAppropriateLayoutGravity(),
                                certificateName = tab.content.securityInfo.issuer,
                                permissionHighlights = tab.content.permissionHighlights,
                                isTrackingProtectionEnabled = isTrackingProtectionEnabled,
                                cookieBannerUIMode = cookieBannerUIMode,
                            )
                        nav(R.id.browserFragment, directions)
                    }
                }
            }
        }
    }

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(
            title: String,
            sessions: List<TabSessionState>,
            id: Long?,
        ) {
            showTabSavedToCollectionSnackbar(sessions.size, true)
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<TabSessionState>) {
            showTabSavedToCollectionSnackbar(sessions.size)
        }

        private fun showTabSavedToCollectionSnackbar(
            tabSize: Int,
            isNewCollection: Boolean = false,
        ) {
            view?.let { view ->
                val messageStringRes = when {
                    isNewCollection -> {
                        R.string.create_collection_tabs_saved_new_collection
                    }
                    tabSize > 1 -> {
                        R.string.create_collection_tabs_saved
                    }
                    else -> {
                        R.string.create_collection_tab_saved
                    }
                }
                FenixSnackbar.make(
                    view = binding.dynamicSnackbarContainer,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true,
                )
                    .setText(view.context.getString(messageStringRes))
                    .setAction(requireContext().getString(R.string.create_collection_view)) {
                        findNavController().navigate(
                            BrowserFragmentDirections.actionGlobalHome(
                                focusOnAddressBar = false,
                                scrollToCollection = true,
                            ),
                        )
                    }
                    .show()
            }
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View,
    ): List<ContextMenuCandidate> {
        val contextMenuCandidateAppLinksUseCases = AppLinksUseCases(
            requireContext(),
            { true },
        )

        return ContextMenuCandidate.defaultCandidates(
            context,
            context.components.useCases.tabsUseCases,
            context.components.useCases.contextMenuUseCases,
            view,
            FenixSnackbarDelegate(view),
        ) + ContextMenuCandidate.createOpenInExternalAppCandidate(
            requireContext(),
            contextMenuCandidateAppLinksUseCases,
        )
    }

    /**
     * Updates the last time the user was active on the [BrowserFragment].
     * This is useful to determine if the user has to start on the [HomeFragment]
     * or it should go directly to the [BrowserFragment].
     */
    @VisibleForTesting
    internal fun updateLastBrowseActivity() {
        requireContext().settings().lastBrowseActivity = System.currentTimeMillis()
    }

    companion object {
        /** Indicates weight of an action. The lesser the weight, the closer it is to the url.
         * A default weight -1 indicates, the position is not cared for
         * and action will be appended at the end.
         */
        const val READER_MODE_WEIGHT = 1
        const val TRANSLATIONS_WEIGHT = 2
        const val REVIEW_QUALITY_CHECK_WEIGHT = 3
        const val SHARE_WEIGHT = 4
        const val RELOAD_WEIGHT = 5
    }
}
