/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.coroutines.Job
import org.mozilla.fenix.home.sessioncontrol.viewholders.SaveTabGroupViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoTabMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescriptionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoCollectionMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabInCollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFinishViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFirefoxAccountViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivacyNoticeViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivateBrowsingViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingSectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingThemePickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingTrackingProtectionViewHolder
import mozilla.components.feature.tab.collections.Tab as ComponentTab
import java.lang.IllegalStateException

sealed class AdapterItem {
    data class TabHeader(val isPrivate: Boolean, val hasTabs: Boolean) : AdapterItem()
    object NoTabMessage : AdapterItem()
    data class TabItem(val tab: Tab) : AdapterItem()
    object SaveTabGroup : AdapterItem()

    object PrivateBrowsingDescription : AdapterItem()

    object CollectionHeader : AdapterItem()
    object NoCollectionMessage : AdapterItem()
    data class CollectionItem(val collection: TabCollection) : AdapterItem()
    data class TabInCollectionItem(
        val collection: TabCollection,
        val tab: ComponentTab,
        val isLastTab: Boolean
    ) : AdapterItem()

    object OnboardingHeader : AdapterItem()
    data class OnboardingSectionHeader(val labelBuilder: (Context) -> String) : AdapterItem()
    data class OnboardingFirefoxAccount(val state: OnboardingState) : AdapterItem()
    object OnboardingThemePicker : AdapterItem()
    object OnboardingTrackingProtection : AdapterItem()
    object OnboardingPrivateBrowsing : AdapterItem()
    object OnboardingPrivacyNotice : AdapterItem()
    object OnboardingFinish : AdapterItem()

    val viewType: Int
        get() = when (this) {
            is TabHeader -> TabHeaderViewHolder.LAYOUT_ID
            NoTabMessage -> NoTabMessageViewHolder.LAYOUT_ID
            is TabItem -> TabViewHolder.LAYOUT_ID
            SaveTabGroup -> SaveTabGroupViewHolder.LAYOUT_ID
            PrivateBrowsingDescription -> PrivateBrowsingDescriptionViewHolder.LAYOUT_ID
            CollectionHeader -> CollectionHeaderViewHolder.LAYOUT_ID
            NoCollectionMessage -> NoCollectionMessageViewHolder.LAYOUT_ID
            is CollectionItem -> CollectionViewHolder.LAYOUT_ID
            is TabInCollectionItem -> TabInCollectionViewHolder.LAYOUT_ID
            OnboardingHeader -> OnboardingHeaderViewHolder.LAYOUT_ID
            is OnboardingSectionHeader -> OnboardingSectionHeaderViewHolder.LAYOUT_ID
            is OnboardingFirefoxAccount -> OnboardingFirefoxAccountViewHolder.LAYOUT_ID
            OnboardingThemePicker -> OnboardingThemePickerViewHolder.LAYOUT_ID
            OnboardingTrackingProtection -> OnboardingTrackingProtectionViewHolder.LAYOUT_ID
            OnboardingPrivateBrowsing -> OnboardingPrivateBrowsingViewHolder.LAYOUT_ID
            OnboardingPrivacyNotice -> OnboardingPrivacyNoticeViewHolder.LAYOUT_ID
            OnboardingFinish -> OnboardingFinishViewHolder.LAYOUT_ID
        }
}

class SessionControlAdapter(
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    private var items: List<AdapterItem> = listOf()
    private lateinit var job: Job
    private lateinit var expandedCollections: Set<Long>

    fun reloadData(items: List<AdapterItem>, expandedCollections: Set<Long>) {
        this.items = items
        this.expandedCollections = expandedCollections
        notifyDataSetChanged()
    }

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            TabHeaderViewHolder.LAYOUT_ID -> TabHeaderViewHolder(view, actionEmitter)
            NoTabMessageViewHolder.LAYOUT_ID -> NoTabMessageViewHolder(view)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(view, actionEmitter, job)
            SaveTabGroupViewHolder.LAYOUT_ID -> SaveTabGroupViewHolder(view, actionEmitter)
            PrivateBrowsingDescriptionViewHolder.LAYOUT_ID -> PrivateBrowsingDescriptionViewHolder(view, actionEmitter)
            CollectionHeaderViewHolder.LAYOUT_ID -> CollectionHeaderViewHolder(view)
            NoCollectionMessageViewHolder.LAYOUT_ID -> NoCollectionMessageViewHolder(view)
            CollectionViewHolder.LAYOUT_ID -> CollectionViewHolder(view, actionEmitter, job)
            TabInCollectionViewHolder.LAYOUT_ID -> TabInCollectionViewHolder(view, actionEmitter, job)
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

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
    }

    override fun getItemViewType(position: Int) = items[position].viewType

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is TabHeaderViewHolder -> {
                val tabHeader = items[position] as AdapterItem.TabHeader
                holder.bind(tabHeader.isPrivate, tabHeader.hasTabs)
            }
            is TabViewHolder -> holder.bindSession(
                (items[position] as AdapterItem.TabItem).tab
            )
            is CollectionViewHolder -> {
                val collection = (items[position] as AdapterItem.CollectionItem).collection
                holder.bindSession(collection, expandedCollections.contains(collection.id))
            }
            is TabInCollectionViewHolder -> {
                val item = items[position] as AdapterItem.TabInCollectionItem
                holder.bindSession(item.collection, item.tab, item.isLastTab)
            }
            is OnboardingSectionHeaderViewHolder -> holder.bind(
                (items[position] as AdapterItem.OnboardingSectionHeader).labelBuilder
            )
            is OnboardingFirefoxAccountViewHolder -> holder.bind(
                (items[position] as AdapterItem.OnboardingFirefoxAccount).state == OnboardingState.AutoSignedIn
            )
        }
    }
}
