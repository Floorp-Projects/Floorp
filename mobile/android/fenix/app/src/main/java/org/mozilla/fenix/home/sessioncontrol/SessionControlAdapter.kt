/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.home.OnboardingState
import org.mozilla.fenix.home.recenttabs.view.RecentTabViewDecorator
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoCollectionsMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescriptionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabInCollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TopSitePagerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.ExperimentDefaultBrowserCardViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingAutomaticSignInViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFinishViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingManualSignInViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivacyNoticeViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivateBrowsingViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingSectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingThemePickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingToolbarPositionPickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingTrackingProtectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingWhatsNewViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabsHeaderViewHolder
import org.mozilla.fenix.home.recentbookmarks.view.RecentBookmarksViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabsItemPosition
import org.mozilla.fenix.home.tips.ButtonTipViewHolder
import mozilla.components.feature.tab.collections.Tab as ComponentTab

sealed class AdapterItem(@LayoutRes val viewType: Int) {
    data class TipItem(val tip: Tip) : AdapterItem(
        ButtonTipViewHolder.LAYOUT_ID
    )

    /**
     * Contains a set of [Pair]s where [Pair.first] is the index of the changed [TopSite] and
     * [Pair.second] is the new [TopSite].
     */
    data class TopSitePagerPayload(
        val changed: Set<Pair<Int, TopSite>>
    )

    data class TopSitePager(val topSites: List<TopSite>) :
        AdapterItem(TopSitePagerViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem): Boolean {
            val newTopSites = (other as? TopSitePager) ?: return false
            return newTopSites.topSites.size == this.topSites.size
        }

        override fun contentsSameAs(other: AdapterItem): Boolean {
            val newTopSites = (other as? TopSitePager) ?: return false
            if (newTopSites.topSites.size != this.topSites.size) return false
            val newSitesSequence = newTopSites.topSites.asSequence()
            val oldTopSites = this.topSites.asSequence()
            return newSitesSequence.zip(oldTopSites).all { (new, old) -> new == old }
        }

        override fun getChangePayload(newItem: AdapterItem): Any? {
            val newTopSites = (newItem as? TopSitePager) ?: return null
            val oldTopSites = (this as? TopSitePager) ?: return null

            val changed = mutableSetOf<Pair<Int, TopSite>>()
            for ((index, item) in newTopSites.topSites.withIndex()) {
                if (oldTopSites.topSites.getOrNull(index) != item) {
                    changed.add(Pair(index, item))
                }
            }
            return if (changed.isNotEmpty()) TopSitePagerPayload(changed) else null
        }
    }

    object PrivateBrowsingDescription : AdapterItem(PrivateBrowsingDescriptionViewHolder.LAYOUT_ID)
    object NoCollectionsMessage : AdapterItem(NoCollectionsMessageViewHolder.LAYOUT_ID)

    object CollectionHeader : AdapterItem(CollectionHeaderViewHolder.LAYOUT_ID)
    data class CollectionItem(
        val collection: TabCollection,
        val expanded: Boolean
    ) : AdapterItem(CollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is CollectionItem && collection.id == other.collection.id

        override fun contentsSameAs(other: AdapterItem): Boolean {
            (other as? CollectionItem)?.let {
                return it.expanded == this.expanded && it.collection.title == this.collection.title
            } ?: return false
        }
    }

    data class TabInCollectionItem(
        val collection: TabCollection,
        val tab: ComponentTab,
        val isLastTab: Boolean
    ) : AdapterItem(TabInCollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is TabInCollectionItem && tab.id == other.tab.id
    }

    object OnboardingHeader : AdapterItem(OnboardingHeaderViewHolder.LAYOUT_ID)
    data class OnboardingSectionHeader(
        val labelBuilder: (Context) -> String
    ) : AdapterItem(OnboardingSectionHeaderViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is OnboardingSectionHeader && labelBuilder == other.labelBuilder
    }

    object OnboardingManualSignIn : AdapterItem(OnboardingManualSignInViewHolder.LAYOUT_ID)
    data class OnboardingAutomaticSignIn(
        val state: OnboardingState.SignedOutCanAutoSignIn
    ) : AdapterItem(OnboardingAutomaticSignInViewHolder.LAYOUT_ID)

    object ExperimentDefaultBrowserCard : AdapterItem(ExperimentDefaultBrowserCardViewHolder.LAYOUT_ID)

    object OnboardingThemePicker : AdapterItem(OnboardingThemePickerViewHolder.LAYOUT_ID)
    object OnboardingTrackingProtection :
        AdapterItem(OnboardingTrackingProtectionViewHolder.LAYOUT_ID)

    object OnboardingPrivateBrowsing : AdapterItem(OnboardingPrivateBrowsingViewHolder.LAYOUT_ID)
    object OnboardingPrivacyNotice : AdapterItem(OnboardingPrivacyNoticeViewHolder.LAYOUT_ID)
    object OnboardingFinish : AdapterItem(OnboardingFinishViewHolder.LAYOUT_ID)
    object OnboardingToolbarPositionPicker :
        AdapterItem(OnboardingToolbarPositionPickerViewHolder.LAYOUT_ID)

    object OnboardingWhatsNew : AdapterItem(OnboardingWhatsNewViewHolder.LAYOUT_ID)

    object RecentTabsHeader : AdapterItem(RecentTabsHeaderViewHolder.LAYOUT_ID)
    data class RecentTabItem(
        val tab: TabSessionState,
        val position: RecentTabsItemPosition
    ) : AdapterItem(RecentTabViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) = other is RecentTabItem && tab.id == other.tab.id &&
            position == other.position

        override fun contentsSameAs(other: AdapterItem): Boolean {
            val otherItem = other as RecentTabItem
            // We only care about updating if the title and icon have changed because that is
            // all we show today. This should be updated if we want to show updates for more.
            return tab.content.title == otherItem.tab.content.title &&
                    tab.content.icon == otherItem.tab.content.icon
        }
    }

    data class RecentBookmarks(val recentBookmarks: List<BookmarkNode>) :
        AdapterItem(RecentBookmarksViewHolder.LAYOUT_ID) {
            override fun sameAs(other: AdapterItem): Boolean {
                val newBookmarks = (other as? RecentBookmarks) ?: return false
                if (newBookmarks.recentBookmarks.size != this.recentBookmarks.size) {
                    return false
                }

                return recentBookmarks.zip(newBookmarks.recentBookmarks).all { (new, old) ->
                    new.guid == old.guid
                }
            }

            override fun contentsSameAs(other: AdapterItem): Boolean {
                val newBookmarks = (other as? RecentBookmarks) ?: return false

                val newBookmarksSequence = newBookmarks.recentBookmarks.asSequence()
                val oldBookmarksList = this.recentBookmarks.asSequence()

                return newBookmarksSequence.zip(oldBookmarksList).all { (new, old) ->
                    new == old
                }
            }
        }

    /**
     * True if this item represents the same value as other. Used by [AdapterItemDiffCallback].
     */
    open fun sameAs(other: AdapterItem) = this::class == other::class

    /**
     * Returns a payload if there's been a change, or null if not
     */
    open fun getChangePayload(newItem: AdapterItem): Any? = null

    open fun contentsSameAs(other: AdapterItem) = this::class == other::class
}

class AdapterItemDiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
    override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
        oldItem.sameAs(newItem)

    @Suppress("DiffUtilEquals")
    override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
        oldItem.contentsSameAs(newItem)

    override fun getChangePayload(oldItem: AdapterItem, newItem: AdapterItem): Any? {
        return oldItem.getChangePayload(newItem) ?: return super.getChangePayload(oldItem, newItem)
    }
}

class SessionControlAdapter(
    private val interactor: SessionControlInteractor,
    private val viewLifecycleOwner: LifecycleOwner,
    private val components: Components
) : ListAdapter<AdapterItem, RecyclerView.ViewHolder>(AdapterItemDiffCallback()) {

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            ButtonTipViewHolder.LAYOUT_ID -> ButtonTipViewHolder(view, interactor)
            TopSitePagerViewHolder.LAYOUT_ID -> TopSitePagerViewHolder(view, interactor)
            PrivateBrowsingDescriptionViewHolder.LAYOUT_ID -> PrivateBrowsingDescriptionViewHolder(
                view,
                interactor
            )
            NoCollectionsMessageViewHolder.LAYOUT_ID ->
                NoCollectionsMessageViewHolder(
                    view,
                    viewLifecycleOwner,
                    components.core.store,
                    interactor
                )
            CollectionHeaderViewHolder.LAYOUT_ID -> CollectionHeaderViewHolder(view)
            CollectionViewHolder.LAYOUT_ID -> CollectionViewHolder(view, interactor)
            TabInCollectionViewHolder.LAYOUT_ID -> TabInCollectionViewHolder(
                view as WidgetSiteItemView,
                interactor
            )
            OnboardingHeaderViewHolder.LAYOUT_ID -> OnboardingHeaderViewHolder(view)
            OnboardingSectionHeaderViewHolder.LAYOUT_ID -> OnboardingSectionHeaderViewHolder(view)
            OnboardingAutomaticSignInViewHolder.LAYOUT_ID -> OnboardingAutomaticSignInViewHolder(
                view
            )
            OnboardingManualSignInViewHolder.LAYOUT_ID -> OnboardingManualSignInViewHolder(view)
            OnboardingThemePickerViewHolder.LAYOUT_ID -> OnboardingThemePickerViewHolder(view)
            OnboardingTrackingProtectionViewHolder.LAYOUT_ID -> OnboardingTrackingProtectionViewHolder(
                view
            )
            OnboardingPrivateBrowsingViewHolder.LAYOUT_ID -> OnboardingPrivateBrowsingViewHolder(
                view,
                interactor
            )
            OnboardingPrivacyNoticeViewHolder.LAYOUT_ID -> OnboardingPrivacyNoticeViewHolder(
                view,
                interactor
            )
            OnboardingFinishViewHolder.LAYOUT_ID -> OnboardingFinishViewHolder(view, interactor)
            OnboardingWhatsNewViewHolder.LAYOUT_ID -> OnboardingWhatsNewViewHolder(view, interactor)
            OnboardingToolbarPositionPickerViewHolder.LAYOUT_ID -> OnboardingToolbarPositionPickerViewHolder(
                view
            )
            ExperimentDefaultBrowserCardViewHolder.LAYOUT_ID -> ExperimentDefaultBrowserCardViewHolder(view, interactor)
            RecentTabsHeaderViewHolder.LAYOUT_ID -> RecentTabsHeaderViewHolder(view, interactor)
            RecentTabViewHolder.LAYOUT_ID -> RecentTabViewHolder(view, interactor)
            RecentBookmarksViewHolder.LAYOUT_ID -> {
                RecentBookmarksViewHolder(view, interactor)
            }

            else -> throw IllegalStateException()
        }
    }

    override fun getItemViewType(position: Int) = getItem(position).viewType

    override fun onBindViewHolder(
        holder: RecyclerView.ViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
        } else {
            when (holder) {
                is TopSitePagerViewHolder -> {
                    if (payloads[0] is AdapterItem.TopSitePagerPayload) {
                        val payload = payloads[0] as AdapterItem.TopSitePagerPayload
                        holder.update(payload)
                    }
                }
            }
        }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val item = getItem(position)
        when (holder) {
            is ButtonTipViewHolder -> {
                val tipItem = item as AdapterItem.TipItem
                holder.bind(tipItem.tip)
            }
            is TopSitePagerViewHolder -> {
                holder.bind((item as AdapterItem.TopSitePager).topSites)
            }
            is CollectionViewHolder -> {
                val (collection, expanded) = item as AdapterItem.CollectionItem
                holder.bindSession(collection, expanded)
            }
            is TabInCollectionViewHolder -> {
                val (collection, tab, isLastTab) = item as AdapterItem.TabInCollectionItem
                holder.bindSession(collection, tab, isLastTab)
            }
            is OnboardingSectionHeaderViewHolder -> holder.bind(
                (item as AdapterItem.OnboardingSectionHeader).labelBuilder
            )
            is OnboardingManualSignInViewHolder -> holder.bind()
            is OnboardingAutomaticSignInViewHolder -> holder.bind(
                (item as AdapterItem.OnboardingAutomaticSignIn).state.withAccount
            )
            is RecentTabViewHolder -> {
                val (tab, tabPosition) = item as AdapterItem.RecentTabItem
                holder.bindTab(tab).apply {
                    RecentTabViewDecorator.forPosition(tabPosition).invoke(this)
                }
            }
            is RecentBookmarksViewHolder -> {
                holder.bind(
                    (item as AdapterItem.RecentBookmarks).recentBookmarks
                )
            }
        }
    }
}
