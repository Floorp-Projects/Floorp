/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.home.BottomSpacerViewHolder
import org.mozilla.fenix.home.TopPlaceholderViewHolder
import org.mozilla.fenix.home.collections.CollectionViewHolder
import org.mozilla.fenix.home.collections.TabInCollectionViewHolder
import org.mozilla.fenix.home.pocket.PocketCategoriesViewHolder
import org.mozilla.fenix.home.pocket.PocketRecommendationsHeaderViewHolder
import org.mozilla.fenix.home.pocket.PocketStoriesViewHolder
import org.mozilla.fenix.home.recentbookmarks.view.RecentBookmarksHeaderViewHolder
import org.mozilla.fenix.home.recentbookmarks.view.RecentBookmarksViewHolder
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTabViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabsHeaderViewHolder
import org.mozilla.fenix.home.recentvisits.view.RecentVisitsHeaderViewHolder
import org.mozilla.fenix.home.recentvisits.view.RecentlyVisitedViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CustomizeHomeButtonViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoCollectionsMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescriptionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.MessageCardViewHolder
import org.mozilla.fenix.home.topsites.TopSitePagerViewHolder
import org.mozilla.fenix.home.topsites.TopSitesViewHolder
import mozilla.components.feature.tab.collections.Tab as ComponentTab

sealed class AdapterItem(@LayoutRes val viewType: Int) {
    object TopPlaceholderItem : AdapterItem(TopPlaceholderViewHolder.LAYOUT_ID)

    /**
     * Top sites.
     */
    object TopSites : AdapterItem(TopSitesViewHolder.LAYOUT_ID)

    /**
     * Contains a set of [Pair]s where [Pair.first] is the index of the changed [TopSite] and
     * [Pair.second] is the new [TopSite].
     */
    data class TopSitePagerPayload(
        val changed: Set<Pair<Int, TopSite>>,
    )

    data class TopSitePager(val topSites: List<TopSite>) :
        AdapterItem(TopSitePagerViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem): Boolean {
            return other is TopSitePager
        }

        override fun contentsSameAs(other: AdapterItem): Boolean {
            val newTopSites = (other as? TopSitePager) ?: return false
            if (newTopSites.topSites.size != this.topSites.size) return false
            val newSitesSequence = newTopSites.topSites.asSequence()
            val oldTopSites = this.topSites.asSequence()
            return newSitesSequence.zip(oldTopSites).all { (new, old) -> new == old }
        }

        /**
         * Returns a payload if there's been a change, or null if not, but adds a "dummy" item for
         * each deleted [TopSite]. This is done in order to more easily identify the actual views
         * that need to be removed in [TopSitesPagerAdapter.update].
         *
         * See https://github.com/mozilla-mobile/fenix/pull/20189#issuecomment-877124730
         */
        @Suppress("ComplexCondition")
        override fun getChangePayload(newItem: AdapterItem): Any? {
            val newTopSites = (newItem as? TopSitePager)
            val oldTopSites = (this as? TopSitePager)

            if (newTopSites == null || oldTopSites == null ||
                newTopSites.topSites.size > oldTopSites.topSites.size ||
                (newTopSites.topSites.size > TopSitePagerViewHolder.TOP_SITES_PER_PAGE)
                != (oldTopSites.topSites.size > TopSitePagerViewHolder.TOP_SITES_PER_PAGE)
            ) {
                return null
            }

            val changed = mutableSetOf<Pair<Int, TopSite>>()

            for ((index, item) in oldTopSites.topSites.withIndex()) {
                val changedItem =
                    newTopSites.topSites.getOrNull(index) ?: TopSite.Frecent(-1, "REMOVED", "", 0)
                if (changedItem != item) {
                    changed.add((Pair(index, changedItem)))
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
        val expanded: Boolean,
    ) : AdapterItem(CollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is CollectionItem && collection.id == other.collection.id

        override fun contentsSameAs(other: AdapterItem): Boolean {
            (other as? CollectionItem)?.let {
                return it.expanded == this.expanded &&
                    it.collection.title == this.collection.title &&
                    it.collection.tabs == this.collection.tabs
            } ?: return false
        }
    }

    data class TabInCollectionItem(
        val collection: TabCollection,
        val tab: ComponentTab,
        val isLastTab: Boolean,
    ) : AdapterItem(TabInCollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is TabInCollectionItem && tab.id == other.tab.id

        override fun contentsSameAs(other: AdapterItem): Boolean {
            return other is TabInCollectionItem && this.isLastTab == other.isLastTab
        }
    }

    data class NimbusMessageCard(
        val message: Message,
    ) : AdapterItem(MessageCardViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is NimbusMessageCard && message.id == other.message.id
    }

    object CustomizeHomeButton : AdapterItem(CustomizeHomeButtonViewHolder.LAYOUT_ID)

    object RecentTabsHeader : AdapterItem(RecentTabsHeaderViewHolder.LAYOUT_ID)
    object RecentTabItem : AdapterItem(RecentTabViewHolder.LAYOUT_ID)

    /**
     * Adapter item to hold homescreen synced tabs view.
     */
    object RecentSyncedTabItem : AdapterItem(RecentSyncedTabViewHolder.LAYOUT_ID)

    object RecentVisitsHeader : AdapterItem(RecentVisitsHeaderViewHolder.LAYOUT_ID)
    object RecentVisitsItems : AdapterItem(RecentlyVisitedViewHolder.LAYOUT_ID)

    object RecentBookmarksHeader : AdapterItem(RecentBookmarksHeaderViewHolder.LAYOUT_ID)
    object RecentBookmarks : AdapterItem(RecentBookmarksViewHolder.LAYOUT_ID)

    object PocketStoriesItem : AdapterItem(PocketStoriesViewHolder.LAYOUT_ID)
    object PocketCategoriesItem : AdapterItem(PocketCategoriesViewHolder.LAYOUT_ID)
    object PocketRecommendationsFooterItem : AdapterItem(PocketRecommendationsHeaderViewHolder.LAYOUT_ID)

    object BottomSpacer : AdapterItem(BottomSpacerViewHolder.LAYOUT_ID)

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
    private val components: Components,
) : ListAdapter<AdapterItem, RecyclerView.ViewHolder>(AdapterItemDiffCallback()) {

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod", "LongMethod", "ReturnCount")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        when (viewType) {
            CustomizeHomeButtonViewHolder.LAYOUT_ID -> return CustomizeHomeButtonViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            MessageCardViewHolder.LAYOUT_ID -> return MessageCardViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            PrivateBrowsingDescriptionViewHolder.LAYOUT_ID -> return PrivateBrowsingDescriptionViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            PocketStoriesViewHolder.LAYOUT_ID -> return PocketStoriesViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            PocketCategoriesViewHolder.LAYOUT_ID -> return PocketCategoriesViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            PocketRecommendationsHeaderViewHolder.LAYOUT_ID -> return PocketRecommendationsHeaderViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            RecentBookmarksViewHolder.LAYOUT_ID -> return RecentBookmarksViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            RecentTabViewHolder.LAYOUT_ID -> return RecentTabViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                recentTabInteractor = interactor,
            )
            RecentSyncedTabViewHolder.LAYOUT_ID -> return RecentSyncedTabViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                recentSyncedTabInteractor = interactor,
            )
            RecentlyVisitedViewHolder.LAYOUT_ID -> return RecentlyVisitedViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            RecentVisitsHeaderViewHolder.LAYOUT_ID -> return RecentVisitsHeaderViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            RecentBookmarksHeaderViewHolder.LAYOUT_ID -> return RecentBookmarksHeaderViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            RecentTabsHeaderViewHolder.LAYOUT_ID -> return RecentTabsHeaderViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            CollectionHeaderViewHolder.LAYOUT_ID -> return CollectionHeaderViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
            )
            CollectionViewHolder.LAYOUT_ID -> return CollectionViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            TabInCollectionViewHolder.LAYOUT_ID -> return TabInCollectionViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            TopSitesViewHolder.LAYOUT_ID -> return TopSitesViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
        }

        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            TopPlaceholderViewHolder.LAYOUT_ID -> TopPlaceholderViewHolder(view)
            TopSitePagerViewHolder.LAYOUT_ID -> TopSitePagerViewHolder(
                view = view,
                appStore = components.appStore,
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            NoCollectionsMessageViewHolder.LAYOUT_ID ->
                NoCollectionsMessageViewHolder(
                    view,
                    viewLifecycleOwner,
                    components.core.store,
                    components.appStore,
                    interactor,
                )
            BottomSpacerViewHolder.LAYOUT_ID -> BottomSpacerViewHolder(view)
            else -> throw IllegalStateException()
        }
    }

    override fun onViewRecycled(holder: RecyclerView.ViewHolder) {
        when (holder) {
            is CollectionHeaderViewHolder,
            is CustomizeHomeButtonViewHolder,
            is RecentlyVisitedViewHolder,
            is RecentVisitsHeaderViewHolder,
            is RecentBookmarksViewHolder,
            is RecentBookmarksHeaderViewHolder,
            is RecentTabViewHolder,
            is RecentSyncedTabViewHolder,
            is RecentTabsHeaderViewHolder,
            is PrivateBrowsingDescriptionViewHolder,
            is PocketCategoriesViewHolder,
            is PocketRecommendationsHeaderViewHolder,
            is PocketStoriesViewHolder,
            is TopSitesViewHolder,
            -> {
                // no op
                // This previously called "composeView.disposeComposition" which would have the
                // entire Composable destroyed and recreated when this View is scrolled off or on screen again.
                // This View already listens and maps store updates. Avoid creating and binding new Views.
                // The composition will live until the ViewTreeLifecycleOwner to which it's attached to is destroyed.
            }
            is CollectionViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            is MessageCardViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            is TabInCollectionViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            else -> super.onViewRecycled(holder)
        }
    }

    override fun getItemViewType(position: Int) = getItem(position).viewType

    override fun onBindViewHolder(
        holder: RecyclerView.ViewHolder,
        position: Int,
        payloads: MutableList<Any>,
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
            is TopPlaceholderViewHolder -> {
                holder.bind()
            }
            is TopSitePagerViewHolder -> {
                holder.bind((item as AdapterItem.TopSitePager).topSites)
            }
            is MessageCardViewHolder -> {
                holder.bind((item as AdapterItem.NimbusMessageCard).message)
            }
            is CollectionViewHolder -> {
                val (collection, expanded) = item as AdapterItem.CollectionItem
                holder.bindSession(collection, expanded)
            }
            is TabInCollectionViewHolder -> {
                val (collection, tab, isLastTab) = item as AdapterItem.TabInCollectionItem
                holder.bindSession(collection, tab, isLastTab)
            }
            is TopSitesViewHolder,
            is RecentlyVisitedViewHolder,
            is RecentBookmarksViewHolder,
            is RecentTabViewHolder,
            is RecentSyncedTabViewHolder,
            is PocketStoriesViewHolder,
            -> {
                // no-op. This ViewHolder receives the HomeStore as argument and will observe that
                // without the need for us to manually update from here the data to be displayed.
            }
        }
    }
}
