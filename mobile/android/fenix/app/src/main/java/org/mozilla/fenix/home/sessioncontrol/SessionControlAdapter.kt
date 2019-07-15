/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.DrawableRes
import androidx.annotation.LayoutRes
import androidx.annotation.StringRes
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoContentMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescriptionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.SaveTabGroupViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabInCollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFinishViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFirefoxAccountViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivacyNoticeViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivateBrowsingViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingSectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingThemePickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingTrackingProtectionViewHolder
import org.mozilla.fenix.utils.ListAdapterWithJob
import mozilla.components.feature.tab.collections.Tab as ComponentTab

sealed class AdapterItem(@LayoutRes val viewType: Int) {
    data class TabHeader(val isPrivate: Boolean, val hasTabs: Boolean) : AdapterItem(TabHeaderViewHolder.LAYOUT_ID)
    data class TabItem(val tab: Tab) : AdapterItem(TabViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) = other is TabItem && tab.sessionId == other.tab.sessionId
    }
    object SaveTabGroup : AdapterItem(SaveTabGroupViewHolder.LAYOUT_ID)

    object PrivateBrowsingDescription : AdapterItem(PrivateBrowsingDescriptionViewHolder.LAYOUT_ID)
    data class NoContentMessage(
        @DrawableRes val icon: Int,
        @StringRes val header: Int,
        @StringRes val description: Int
    ) : AdapterItem(NoContentMessageViewHolder.LAYOUT_ID)

    object CollectionHeader : AdapterItem(CollectionHeaderViewHolder.LAYOUT_ID)
    data class CollectionItem(
        val collection: TabCollection,
        val expanded: Boolean
    ) : AdapterItem(CollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) = other is CollectionItem && collection.id == other.collection.id
    }
    data class TabInCollectionItem(
        val collection: TabCollection,
        val tab: ComponentTab,
        val isLastTab: Boolean
    ) : AdapterItem(TabInCollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) = other is TabInCollectionItem && tab.id == other.tab.id
    }

    object OnboardingHeader : AdapterItem(OnboardingHeaderViewHolder.LAYOUT_ID)
    data class OnboardingSectionHeader(
        val labelBuilder: (Context) -> String
    ) : AdapterItem(OnboardingSectionHeaderViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) = other is OnboardingSectionHeader && labelBuilder == other.labelBuilder
    }
    data class OnboardingFirefoxAccount(
        val state: OnboardingState
    ) : AdapterItem(OnboardingFirefoxAccountViewHolder.LAYOUT_ID)
    object OnboardingThemePicker : AdapterItem(OnboardingThemePickerViewHolder.LAYOUT_ID)
    object OnboardingTrackingProtection : AdapterItem(OnboardingTrackingProtectionViewHolder.LAYOUT_ID)
    object OnboardingPrivateBrowsing : AdapterItem(OnboardingPrivateBrowsingViewHolder.LAYOUT_ID)
    object OnboardingPrivacyNotice : AdapterItem(OnboardingPrivacyNoticeViewHolder.LAYOUT_ID)
    object OnboardingFinish : AdapterItem(OnboardingFinishViewHolder.LAYOUT_ID)

    /**
     * True if this item represents the same value as other. Used by [AdapterItemDiffCallback].
     */
    open fun sameAs(other: AdapterItem) = this::class == other::class
}

class AdapterItemDiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
    override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) = oldItem.sameAs(newItem)

    @Suppress("DiffUtilEquals")
    override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) = oldItem == newItem
}

class SessionControlAdapter(
    private val actionEmitter: Observer<SessionControlAction>
) : ListAdapterWithJob<AdapterItem, RecyclerView.ViewHolder>(AdapterItemDiffCallback()) {

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            TabHeaderViewHolder.LAYOUT_ID -> TabHeaderViewHolder(view, actionEmitter)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(view, actionEmitter, adapterJob)
            SaveTabGroupViewHolder.LAYOUT_ID -> SaveTabGroupViewHolder(view, actionEmitter)
            PrivateBrowsingDescriptionViewHolder.LAYOUT_ID -> PrivateBrowsingDescriptionViewHolder(view, actionEmitter)
            NoContentMessageViewHolder.LAYOUT_ID -> NoContentMessageViewHolder(view)
            CollectionHeaderViewHolder.LAYOUT_ID -> CollectionHeaderViewHolder(view)
            CollectionViewHolder.LAYOUT_ID -> CollectionViewHolder(view, actionEmitter)
            TabInCollectionViewHolder.LAYOUT_ID -> TabInCollectionViewHolder(view, actionEmitter, adapterJob)
            OnboardingHeaderViewHolder.LAYOUT_ID -> OnboardingHeaderViewHolder(view)
            OnboardingSectionHeaderViewHolder.LAYOUT_ID -> OnboardingSectionHeaderViewHolder(view)
            OnboardingFirefoxAccountViewHolder.LAYOUT_ID -> OnboardingFirefoxAccountViewHolder(view)
            OnboardingThemePickerViewHolder.LAYOUT_ID -> OnboardingThemePickerViewHolder(view)
            OnboardingTrackingProtectionViewHolder.LAYOUT_ID -> OnboardingTrackingProtectionViewHolder(view)
            OnboardingPrivateBrowsingViewHolder.LAYOUT_ID -> OnboardingPrivateBrowsingViewHolder(view)
            OnboardingPrivacyNoticeViewHolder.LAYOUT_ID -> OnboardingPrivacyNoticeViewHolder(view)
            OnboardingFinishViewHolder.LAYOUT_ID -> OnboardingFinishViewHolder(view, actionEmitter)
            else -> throw IllegalStateException()
        }
    }

    override fun getItemViewType(position: Int) = getItem(position).viewType

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val item = getItem(position)
        when (holder) {
            is TabHeaderViewHolder -> {
                val tabHeader = item as AdapterItem.TabHeader
                holder.bind(tabHeader.isPrivate, tabHeader.hasTabs)
            }
            is TabViewHolder -> holder.bindSession(
                (item as AdapterItem.TabItem).tab
            )
            is NoContentMessageViewHolder -> {
                val (icon, header, description) = item as AdapterItem.NoContentMessage
                holder.bind(icon, header, description)
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
            is OnboardingFirefoxAccountViewHolder -> holder.bind(
                (item as AdapterItem.OnboardingFirefoxAccount).state == OnboardingState.AutoSignedIn
            )
        }
    }
}
