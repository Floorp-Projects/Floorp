/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import androidx.annotation.IdRes
import androidx.cardview.widget.CardView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import androidx.core.view.updatePadding
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray_fab.view.*
import kotlinx.android.synthetic.main.tabs_tray_tab_counter.*
import kotlinx.android.synthetic.main.tabstray_multiselect_items.view.*
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.feature.syncedtabs.SyncedTabsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.InfoBanner
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.TabCounter.Companion.INFINITE_CHAR_PADDING_BOTTOM
import org.mozilla.fenix.components.toolbar.TabCounter.Companion.MAX_VISIBLE_TABS
import org.mozilla.fenix.components.toolbar.TabCounter.Companion.SO_MANY_TABS_OPEN
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.updateAccessibilityCollectionInfo
import org.mozilla.fenix.tabtray.SaveToCollectionsButtonAdapter.MultiselectModeChange
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentState.Mode
import java.text.NumberFormat
import kotlin.math.max
import kotlin.math.roundToInt
import mozilla.components.browser.storage.sync.Tab as SyncTab

/**
 * View that contains and configures the BrowserAwesomeBar
 */
@Suppress("LongParameterList", "TooManyFunctions", "LargeClass")
class TabTrayView(
    private val container: ViewGroup,
    private val tabsAdapter: FenixTabsAdapter,
    private val interactor: TabTrayInteractor,
    store: TabTrayDialogFragmentStore,
    isPrivate: Boolean,
    private val isInLandscape: () -> Boolean,
    lifecycleOwner: LifecycleOwner,
    private val filterTabs: (Boolean) -> Unit
) : LayoutContainer, TabLayout.OnTabSelectedListener {
    val lifecycleScope = lifecycleOwner.lifecycleScope
    val fabView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray_fab, container, true)

    private val hasAccessibilityEnabled = container.context.settings().accessibilityServicesEnabled

    val view = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray, container, true)

    private val isPrivateModeSelected: Boolean get() = view.tab_layout.selectedTabPosition == PRIVATE_TAB_ID

    private val behavior = BottomSheetBehavior.from(view.tab_wrapper)

    private val concatAdapter = ConcatAdapter(tabsAdapter)
    private val tabTrayItemMenu: TabTrayItemMenu
    private var menu: BrowserMenu? = null

    private val multiselectSelectionMenu: MultiselectSelectionMenu
    private var multiselectMenu: BrowserMenu? = null

    private var tabsTouchHelper: TabsTouchHelper
    private val collectionsButtonAdapter = SaveToCollectionsButtonAdapter(interactor, isPrivate)

    private val syncedTabsController =
        SyncedTabsController(lifecycleOwner, view, store, concatAdapter)
    private val syncedTabsFeature = ViewBoundFeatureWrapper<SyncedTabsFeature>()

    private var hasLoaded = false

    override val containerView: View?
        get() = container

    private val components = container.context.components

    private val checkOpenTabs = {
        if (isPrivateModeSelected) {
            view.context.components.core.store.state.privateTabs.isNotEmpty()
        } else {
            view.context.components.core.store.state.normalTabs.isNotEmpty()
        }
    }

    init {
        components.analytics.metrics.track(Event.TabsTrayOpened)

        toggleFabText(isPrivate)

        view.topBar.setOnClickListener {
            // no-op, consume the touch event to prevent it advancing the tray to the next state.
        }
        behavior.addBottomSheetCallback(object : BottomSheetBehavior.BottomSheetCallback() {
            override fun onSlide(bottomSheet: View, slideOffset: Float) {
                if (interactor.onModeRequested() is Mode.Normal && !hasAccessibilityEnabled) {
                    if (slideOffset >= SLIDE_OFFSET) {
                        fabView.new_tab_button.show()
                    } else {
                        fabView.new_tab_button.hide()
                    }
                }
            }

            override fun onStateChanged(bottomSheet: View, newState: Int) {
                if (newState == BottomSheetBehavior.STATE_HIDDEN) {
                    components.analytics.metrics.track(Event.TabsTrayClosed)
                    interactor.onTabTrayDismissed()
                }
                // We only support expanded and collapsed states. Don't allow STATE_HALF_EXPANDED.
                else if (newState == BottomSheetBehavior.STATE_HALF_EXPANDED) {
                    behavior.state = BottomSheetBehavior.STATE_HIDDEN
                }
            }
        })

        val selectedTabIndex = if (!isPrivate) {
            DEFAULT_TAB_ID
        } else {
            PRIVATE_TAB_ID
        }

        view.tab_layout.getTabAt(selectedTabIndex)?.also {
            view.tab_layout.selectTab(it, true)
        }

        view.tab_layout.addOnTabSelectedListener(this)

        val tabs = getTabs(isPrivate)

        updateBottomSheetBehavior()

        setTopOffset(isInLandscape())

        if (view.context.settings().syncedTabsInTabsTray) {
            syncedTabsFeature.set(
                feature = SyncedTabsFeature(
                    context = container.context,
                    storage = components.backgroundServices.syncedTabsStorage,
                    accountManager = components.backgroundServices.accountManager,
                    view = syncedTabsController,
                    lifecycleOwner = lifecycleOwner,
                    onTabClicked = ::handleTabClicked
                ),
                owner = lifecycleOwner,
                view = view
            )
        }

        updateTabsTrayLayout()

        view.tabsTray.apply {
            adapter = concatAdapter

            tabsTouchHelper = TabsTouchHelper(
                observable = tabsAdapter,
                onViewHolderTouched = { it is TabViewHolder }
            )

            tabsTouchHelper.attachToRecyclerView(this)

            tabsAdapter.tabTrayInteractor = interactor
            tabsAdapter.onTabsUpdated = {
                concatAdapter.addAdapter(collectionsButtonAdapter)
                concatAdapter.addAdapter(syncedTabsController.adapter)

                if (hasAccessibilityEnabled) {
                    tabsAdapter.notifyItemRangeChanged(0, tabs.size)
                }
                if (!hasLoaded) {
                    hasLoaded = true
                    scrollToSelectedBrowserTab()
                    if (view.context.settings().accessibilityServicesEnabled) {
                        lifecycleScope.launch {
                            delay(SELECTION_DELAY.toLong())
                            lifecycleScope.launch(Main) {
                                layoutManager?.findViewByPosition(getSelectedBrowserTabViewIndex())
                                    ?.requestFocus()
                                layoutManager?.findViewByPosition(getSelectedBrowserTabViewIndex())
                                    ?.sendAccessibilityEvent(
                                        AccessibilityEvent.TYPE_VIEW_FOCUSED
                                    )
                            }
                        }
                    }
                }
            }
        }

        tabTrayItemMenu =
            TabTrayItemMenu(
                context = view.context,
                shouldShowShareAllTabs = { checkOpenTabs.invoke() && view.tab_layout.selectedTabPosition == 0 },
                shouldShowSelectTabs = { checkOpenTabs.invoke() && view.tab_layout.selectedTabPosition == 0 },
                hasOpenTabs = checkOpenTabs
            ) {
                when (it) {
                    is TabTrayItemMenu.Item.ShareAllTabs -> interactor.onShareTabsOfTypeClicked(
                        isPrivateModeSelected
                    )
                    is TabTrayItemMenu.Item.OpenTabSettings -> interactor.onTabSettingsClicked()
                    is TabTrayItemMenu.Item.SelectTabs -> interactor.onEnterMultiselect()
                    is TabTrayItemMenu.Item.CloseAllTabs -> interactor.onCloseAllTabsClicked(
                        isPrivateModeSelected
                    )
                    is TabTrayItemMenu.Item.OpenRecentlyClosed -> interactor.onOpenRecentlyClosedClicked()
                }
            }

        multiselectSelectionMenu = MultiselectSelectionMenu(
            context = view.context
        ) {
            when (it) {
                is MultiselectSelectionMenu.Item.BookmarkTabs -> interactor.onBookmarkSelectedTabs(
                    mode.selectedItems
                )
                is MultiselectSelectionMenu.Item.DeleteTabs -> interactor.onDeleteSelectedTabs(
                    mode.selectedItems
                )
            }
        }

        view.tab_tray_overflow.setOnClickListener {
            components.analytics.metrics.track(Event.TabsTrayMenuOpened)
            menu = tabTrayItemMenu.menuBuilder.build(container.context)
            menu?.show(it)?.also { popupMenu ->
                (popupMenu.contentView as? CardView)?.setCardBackgroundColor(
                    ContextCompat.getColor(
                        view.context,
                        R.color.foundation_normal_theme
                    )
                )
            }
        }

        adjustNewTabButtonsForNormalMode()

        @Suppress("ComplexCondition")
        if (
            view.context.settings().shouldShowGridViewBanner &&
            view.context.settings().canShowCfr &&
            tabs.size >= TAB_COUNT_SHOW_CFR
        ) {
            InfoBanner(
                context = view.context,
                message = view.context.getString(R.string.tab_tray_grid_view_banner_message),
                dismissText = view.context.getString(R.string.tab_tray_grid_view_banner_negative_button_text),
                actionText = view.context.getString(R.string.tab_tray_grid_view_banner_positive_button_text),
                container = view.infoBanner,
                dismissByHiding = true,
                dismissAction = { view.context.settings().shouldShowGridViewBanner = false }
            ) {
                interactor.onGoToTabsSettings()
                view.context.settings().shouldShowGridViewBanner = false
            }.apply {
                view.infoBanner.visibility = View.VISIBLE
                showBanner()
            }
        } else if (
            view.context.settings().shouldShowAutoCloseTabsBanner &&
            view.context.settings().canShowCfr &&
            tabs.size >= TAB_COUNT_SHOW_CFR
        ) {
            InfoBanner(
                context = view.context,
                message = view.context.getString(R.string.tab_tray_close_tabs_banner_message),
                dismissText = view.context.getString(R.string.tab_tray_close_tabs_banner_negative_button_text),
                actionText = view.context.getString(R.string.tab_tray_close_tabs_banner_positive_button_text),
                container = view.infoBanner,
                dismissByHiding = true,
                dismissAction = { view.context.settings().shouldShowAutoCloseTabsBanner = false }
            ) {
                interactor.onGoToTabsSettings()
                view.context.settings().shouldShowAutoCloseTabsBanner = false
            }.apply {
                view.infoBanner.visibility = View.VISIBLE
                showBanner()
            }
        }
    }

    private fun getTabs(isPrivate: Boolean): List<TabSessionState> = if (isPrivate) {
        view.context.components.core.store.state.privateTabs
    } else {
        view.context.components.core.store.state.normalTabs
    }

    private fun getTabsNumberInAnyMode(): Int {
        return max(
            view.context.components.core.store.state.normalTabs.size,
            view.context.components.core.store.state.privateTabs.size
        )
    }

    private fun getTabsNumberForExpandingTray(): Int {
        return if (container.context.settings().gridTabView) {
            EXPAND_AT_GRID_SIZE
        } else {
            EXPAND_AT_LIST_SIZE
        }
    }

    private fun handleTabClicked(tab: SyncTab) {
        interactor.onSyncedTabClicked(tab)
    }

    private fun adjustNewTabButtonsForNormalMode() {
        view.tab_tray_new_tab.apply {
            isVisible = hasAccessibilityEnabled
            setOnClickListener {
                sendNewTabEvent(isPrivateModeSelected)
                interactor.onNewTabTapped(isPrivateModeSelected)
            }
        }

        fabView.new_tab_button.apply {
            isVisible = !hasAccessibilityEnabled
            setOnClickListener {
                sendNewTabEvent(isPrivateModeSelected)
                interactor.onNewTabTapped(isPrivateModeSelected)
            }
        }
    }

    private fun sendNewTabEvent(isPrivateModeSelected: Boolean) {
        val eventToSend = if (isPrivateModeSelected) {
            Event.NewPrivateTabTapped
        } else {
            Event.NewTabTapped
        }

        components.analytics.metrics.track(eventToSend)
    }

    /**
     * Updates the bottom sheet height based on the number tabs or screen orientation.
     * Show the bottom sheet fully expanded if it is in landscape mode or the number of
     * tabs are greater or equal to the expand size limit.
     */
    fun updateBottomSheetBehavior() {
        if (isInLandscape() || getTabsNumberInAnyMode() >= getTabsNumberForExpandingTray()) {
            behavior.state = BottomSheetBehavior.STATE_EXPANDED
        } else {
            behavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
    }

    enum class TabChange {
        PRIVATE, NORMAL
    }

    private fun toggleSaveToCollectionButton(isPrivate: Boolean) {
        collectionsButtonAdapter.notifyItemChanged(
            0,
            if (isPrivate) TabChange.PRIVATE else TabChange.NORMAL
        )
    }

    override fun onTabSelected(tab: TabLayout.Tab?) {
        toggleFabText(isPrivateModeSelected)
        filterTabs.invoke(isPrivateModeSelected)
        toggleSaveToCollectionButton(isPrivateModeSelected)

        updateUINormalMode(view.context.components.core.store.state)
        scrollToSelectedBrowserTab()

        if (isPrivateModeSelected) {
            components.analytics.metrics.track(Event.TabsTrayPrivateModeTapped)
        } else {
            components.analytics.metrics.track(Event.TabsTrayNormalModeTapped)
        }
    }

    override fun onTabReselected(tab: TabLayout.Tab?) = Unit
    override fun onTabUnselected(tab: TabLayout.Tab?) = Unit

    var mode: Mode = Mode.Normal
        private set

    fun updateTabsTrayLayout() {
        if (container.context.settings().gridTabView) {
            setupGridTabView()
        } else {
            setupListTabView()
        }
    }

    private fun setupGridTabView() {
        view.tabsTray.apply {
            val gridLayoutManager =
                GridLayoutManager(container.context, getNumberOfGridColumns(container.context))

            gridLayoutManager.spanSizeLookup = object : GridLayoutManager.SpanSizeLookup() {
                override fun getSpanSize(position: Int): Int {
                    val numTabs = tabsAdapter.itemCount
                    return if (position < numTabs) {
                        1
                    } else {
                        getNumberOfGridColumns(container.context)
                    }
                }
            }

            layoutManager = gridLayoutManager

            // Ensure items have the same all around padding - 16 dp. Avoid the double spacing issue.
            // A 8dp padding is already set in xml, pad the parent with the remaining needed 8dp.
            updateLayoutParams<ConstraintLayout.LayoutParams> {
                val padding = GRID_ITEM_PARENT_PADDING.dpToPx(resources.displayMetrics)
                // Account for the already set bottom padding needed to accommodate the fab.
                val bottomPadding = paddingBottom + padding
                setPadding(padding, padding, padding, bottomPadding)
            }
        }
    }

    /**
     * Returns the number of columns that will fit in the grid layout for the current screen.
     */
    private fun getNumberOfGridColumns(context: Context): Int {
        val displayMetrics = context.resources.displayMetrics
        val screenWidthDp = displayMetrics.widthPixels / displayMetrics.density
        val columnCount = (screenWidthDp / COLUMN_WIDTH_DP).toInt()
        return if (columnCount >= 2) columnCount else 2
    }

    private fun setupListTabView() {
        view.tabsTray.apply {
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    fun updateState(state: TabTrayDialogFragmentState) {
        val oldMode = mode

        if (oldMode::class != state.mode::class) {
            updateTabsForMultiselectModeChanged(state.mode is Mode.MultiSelect)
            if (view.context.settings().accessibilityServicesEnabled) {
                view.announceForAccessibility(
                    if (state.mode == Mode.Normal) view.context.getString(
                        R.string.tab_tray_exit_multiselect_content_description
                    ) else view.context.getString(R.string.tab_tray_enter_multiselect_content_description)
                )
            }
        }

        mode = state.mode
        when (state.mode) {
            Mode.Normal -> {
                view.tabsTray.apply {
                    tabsTouchHelper.attachToRecyclerView(this)
                }

                toggleUIMultiselect(multiselect = false)

                updateUINormalMode(state.browserState)
            }
            is Mode.MultiSelect -> {
                // Disable swipe to delete while in multiselect
                tabsTouchHelper.attachToRecyclerView(null)

                toggleUIMultiselect(multiselect = true)

                fabView.new_tab_button.isVisible = false
                view.tab_tray_new_tab.isVisible = false
                view.collect_multi_select.isVisible = state.mode.selectedItems.isNotEmpty()
                view.share_multi_select.isVisible = state.mode.selectedItems.isNotEmpty()
                view.menu_multi_select.isVisible = state.mode.selectedItems.isNotEmpty()

                view.multiselect_title.text = view.context.getString(
                    R.string.tab_tray_multi_select_title,
                    state.mode.selectedItems.size
                )
                view.collect_multi_select.setOnClickListener {
                    interactor.onSaveToCollectionClicked(state.mode.selectedItems)
                }
                view.share_multi_select.setOnClickListener {
                    interactor.onShareSelectedTabsClicked(state.mode.selectedItems)
                }
                view.menu_multi_select.setOnClickListener {
                    multiselectMenu = multiselectSelectionMenu.menuBuilder.build(container.context)
                    multiselectMenu?.show(it)?.also { popupMenu ->
                        (popupMenu.contentView as? CardView)?.setCardBackgroundColor(
                            ContextCompat.getColor(
                                view.context,
                                R.color.foundation_normal_theme
                            )
                        )
                    }
                }
                view.exit_multi_select.setOnClickListener {
                    interactor.onBackPressed()
                }
            }
        }

        if (oldMode.selectedItems != state.mode.selectedItems) {
            val unselectedItems = oldMode.selectedItems - state.mode.selectedItems

            state.mode.selectedItems.union(unselectedItems).forEach { item ->
                if (view.context.settings().accessibilityServicesEnabled) {
                    view.announceForAccessibility(
                        if (unselectedItems.contains(item)) view.context.getString(
                            R.string.tab_tray_item_unselected_multiselect_content_description,
                            item.title
                        ) else view.context.getString(
                            R.string.tab_tray_item_selected_multiselect_content_description,
                            item.title
                        )
                    )
                }
                updateTabsForSelectionChanged(item.id)
            }
        }
    }

    private fun ConstraintLayout.setChildWPercent(percentage: Float, @IdRes childId: Int) {
        this.findViewById<View>(childId)?.let {
            val constraintSet = ConstraintSet()
            constraintSet.clone(this)
            constraintSet.constrainPercentWidth(it.id, percentage)
            constraintSet.applyTo(this)
            it.requestLayout()
        }
    }

    private fun updateUINormalMode(browserState: BrowserState) {
        val hasNoTabs = if (isPrivateModeSelected) {
            browserState.privateTabs.isEmpty()
        } else {
            browserState.normalTabs.isEmpty()
        }

        view.tab_tray_empty_view.isVisible = hasNoTabs
        if (hasNoTabs) {
            view.tab_tray_empty_view.text = if (isPrivateModeSelected) {
                view.context.getString(R.string.no_private_tabs_description)
            } else {
                view.context?.getString(R.string.no_open_tabs_description)
            }
        }

        view.tabsTray.visibility = if (hasNoTabs) {
            View.INVISIBLE
        } else {
            View.VISIBLE
        }

        counter_text.text = updateTabCounter(browserState.normalTabs.size)
        updateTabTrayViewAccessibility(browserState.normalTabs.size)

        adjustNewTabButtonsForNormalMode()
    }

    private fun toggleUIMultiselect(multiselect: Boolean) {
        view.multiselect_title.isVisible = multiselect
        view.collect_multi_select.isVisible = multiselect
        view.share_multi_select.isVisible = multiselect
        view.menu_multi_select.isVisible = multiselect
        view.exit_multi_select.isVisible = multiselect

        view.topBar.setBackgroundColor(
            ContextCompat.getColor(
                view.context,
                if (multiselect) R.color.accent_normal_theme else R.color.foundation_normal_theme
            )
        )

        view.handle.updateLayoutParams<ViewGroup.MarginLayoutParams> {
            height = view.resources.getDimensionPixelSize(
                if (multiselect) {
                    R.dimen.tab_tray_multiselect_handle_height
                } else {
                    R.dimen.bottom_sheet_handle_height
                }
            )
            topMargin = view.resources.getDimensionPixelSize(
                if (multiselect) {
                    R.dimen.tab_tray_multiselect_handle_top_margin
                } else {
                    R.dimen.bottom_sheet_handle_top_margin
                }
            )
        }

        view.tab_wrapper.setChildWPercent(
            if (multiselect) 1F else NORMAL_HANDLE_PERCENT_WIDTH,
            view.handle.id
        )

        view.handle.setBackgroundColor(
            ContextCompat.getColor(
                view.context,
                if (multiselect) R.color.accent_normal_theme else R.color.secondary_text_normal_theme
            )
        )

        view.tab_layout.isVisible = !multiselect
        view.tab_tray_empty_view.isVisible = !multiselect
        view.tab_tray_overflow.isVisible = !multiselect
        view.tab_layout.isVisible = !multiselect
    }

    private fun updateTabsForMultiselectModeChanged(inMultiselectMode: Boolean) {
        view.tabsTray.apply {
            val tabs = view.context.components.core.store.state.getNormalOrPrivateTabs(
                isPrivateModeSelected
            )

            collectionsButtonAdapter.notifyItemChanged(
                0,
                if (inMultiselectMode) MultiselectModeChange.MULTISELECT else MultiselectModeChange.NORMAL
            )

            tabsAdapter.notifyItemRangeChanged(0, tabs.size, true)
        }
    }

    private fun updateTabsForSelectionChanged(itemId: String) {
        view.tabsTray.apply {
            val tabs = view.context.components.core.store.state.getNormalOrPrivateTabs(
                isPrivateModeSelected
            )

            val selectedBrowserTabIndex = tabs.indexOfFirst { it.id == itemId }

            tabsAdapter.notifyItemChanged(
                selectedBrowserTabIndex, true
            )
        }
    }

    private fun updateTabTrayViewAccessibility(count: Int) {
        view.tab_layout.getTabAt(0)?.contentDescription = if (count == 1) {
            view.context?.getString(R.string.open_tab_tray_single)
        } else {
            String.format(view.context.getString(R.string.open_tab_tray_plural), count.toString())
        }

        val isListTabView = view.context.settings().listTabView
        val columnCount = if (isListTabView) 1 else getNumberOfGridColumns(view.context)
        val rowCount = count.toDouble().div(columnCount).roundToInt()

        view.tabsTray.updateAccessibilityCollectionInfo(rowCount, columnCount)
    }

    private fun updateTabCounter(count: Int): String {
        if (count > MAX_VISIBLE_TABS) {
            counter_text.updatePadding(bottom = INFINITE_CHAR_PADDING_BOTTOM)
            return SO_MANY_TABS_OPEN
        }
        return NumberFormat.getInstance().format(count.toLong())
    }

    fun setTopOffset(landscape: Boolean) {
        val topOffset = if (landscape) {
            0
        } else {
            view.resources.getDimensionPixelSize(R.dimen.tab_tray_top_offset)
        }

        behavior.setExpandedOffset(topOffset)
    }

    fun dismissMenu() {
        menu?.dismiss()
    }

    private fun toggleFabText(private: Boolean) {
        if (private) {
            fabView.new_tab_button.extend()
            fabView.new_tab_button.contentDescription =
                view.context.getString(R.string.add_private_tab)
        } else {
            fabView.new_tab_button.shrink()
            fabView.new_tab_button.contentDescription =
                view.context.getString(R.string.add_tab)
        }
    }

    fun onBackPressed(): Boolean {
        return interactor.onBackPressed()
    }

    fun scrollToSelectedBrowserTab(selectedTabId: String? = null) {
        view.tabsTray.apply {
            val recyclerViewIndex = getSelectedBrowserTabViewIndex(selectedTabId)

            layoutManager?.scrollToPosition(recyclerViewIndex)
            smoothScrollBy(
                0,
                -resources.getDimensionPixelSize(R.dimen.tab_tray_tab_item_height) / 2
            )
        }
    }

    private fun getSelectedBrowserTabViewIndex(sessionId: String? = null): Int {
        val tabs = if (isPrivateModeSelected) {
            view.context.components.core.store.state.privateTabs
        } else {
            view.context.components.core.store.state.normalTabs
        }

        return if (sessionId != null) {
            tabs.indexOfFirst { it.id == sessionId }
        } else {
            tabs.indexOfFirst { it.id == view.context.components.core.store.state.selectedTabId }
        }
    }

    companion object {
        private const val TAB_COUNT_SHOW_CFR = 6
        private const val DEFAULT_TAB_ID = 0
        private const val PRIVATE_TAB_ID = 1

        // Minimum number of list items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_LIST_SIZE = 4

        // Minimum number of grid items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_GRID_SIZE = 3
        private const val SLIDE_OFFSET = 0
        private const val SELECTION_DELAY = 500
        private const val NORMAL_HANDLE_PERCENT_WIDTH = 0.1F
        private const val COLUMN_WIDTH_DP = 180

        // The remaining padding offset needed to provide a 16dp column spacing between the grid items.
        const val GRID_ITEM_PARENT_PADDING = 8
    }
}
