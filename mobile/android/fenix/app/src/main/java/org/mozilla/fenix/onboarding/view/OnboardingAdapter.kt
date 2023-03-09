/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.home.BottomSpacerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingFinishViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingManualSignInViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingPrivacyNoticeViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingSectionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingThemePickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingToolbarPositionPickerViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.OnboardingTrackingProtectionViewHolder
import org.mozilla.fenix.onboarding.interactor.OnboardingInteractor

/**
 * Adapter for a list of onboarding views to be displayed.
 */
class OnboardingAdapter(
    private val interactor: OnboardingInteractor,
) : ListAdapter<OnboardingAdapterItem, RecyclerView.ViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            OnboardingHeaderViewHolder.LAYOUT_ID -> OnboardingHeaderViewHolder(view)
            OnboardingSectionHeaderViewHolder.LAYOUT_ID -> OnboardingSectionHeaderViewHolder(view)
            OnboardingManualSignInViewHolder.LAYOUT_ID -> OnboardingManualSignInViewHolder(view)
            OnboardingThemePickerViewHolder.LAYOUT_ID -> OnboardingThemePickerViewHolder(view)
            OnboardingTrackingProtectionViewHolder.LAYOUT_ID -> OnboardingTrackingProtectionViewHolder(
                view,
            )
            OnboardingPrivacyNoticeViewHolder.LAYOUT_ID -> OnboardingPrivacyNoticeViewHolder(
                view,
                interactor,
            )
            OnboardingFinishViewHolder.LAYOUT_ID -> OnboardingFinishViewHolder(view, interactor)
            OnboardingToolbarPositionPickerViewHolder.LAYOUT_ID -> OnboardingToolbarPositionPickerViewHolder(
                view,
            )
            BottomSpacerViewHolder.LAYOUT_ID -> BottomSpacerViewHolder(view)
            else -> throw IllegalStateException("ViewType $viewType does not match a ViewHolder")
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val item = getItem(position)

        when (holder) {
            is OnboardingSectionHeaderViewHolder -> holder.bind(
                (item as OnboardingAdapterItem.OnboardingSectionHeader).labelBuilder,
            )
        }
    }

    override fun getItemViewType(position: Int) = getItem(position).viewType

    internal object DiffCallback : DiffUtil.ItemCallback<OnboardingAdapterItem>() {
        override fun areItemsTheSame(oldItem: OnboardingAdapterItem, newItem: OnboardingAdapterItem) =
            oldItem.sameAs(newItem)

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(
            oldItem: OnboardingAdapterItem,
            newItem: OnboardingAdapterItem,
        ) = oldItem.contentsSameAs(newItem)

        override fun getChangePayload(
            oldItem: OnboardingAdapterItem,
            newItem: OnboardingAdapterItem,
        ): Any? {
            return oldItem.getChangePayload(newItem) ?: return super.getChangePayload(oldItem, newItem)
        }
    }
}

/**
 * Enum of the various onboarding views.
 */
sealed class OnboardingAdapterItem(@LayoutRes val viewType: Int) {
    /**
     * Onboarding top header.
     */
    object OnboardingHeader : OnboardingAdapterItem(OnboardingHeaderViewHolder.LAYOUT_ID)

    /**
     * Onboarding section header.
     */
    data class OnboardingSectionHeader(
        val labelBuilder: (Context) -> String,
    ) : OnboardingAdapterItem(OnboardingSectionHeaderViewHolder.LAYOUT_ID) {
        override fun sameAs(other: OnboardingAdapterItem) =
            other is OnboardingSectionHeader && labelBuilder == other.labelBuilder
    }

    /**
     * Onboarding sign into sync card.
     */
    object OnboardingManualSignIn :
        OnboardingAdapterItem(OnboardingManualSignInViewHolder.LAYOUT_ID)

    /**
     * Onboarding theme picker card.
     */
    object OnboardingThemePicker : OnboardingAdapterItem(OnboardingThemePickerViewHolder.LAYOUT_ID)

    /**
     * Onboarding tracking protection card.
     */
    object OnboardingTrackingProtection :
        OnboardingAdapterItem(OnboardingTrackingProtectionViewHolder.LAYOUT_ID)

    /**
     * Onboarding privacy card.
     */
    object OnboardingPrivacyNotice :
        OnboardingAdapterItem(OnboardingPrivacyNoticeViewHolder.LAYOUT_ID)

    /**
     * Onboarding start browsing button.
     */
    object OnboardingFinish : OnboardingAdapterItem(OnboardingFinishViewHolder.LAYOUT_ID)

    /**
     * Onboarding toolbar placement picker.
     */
    object OnboardingToolbarPositionPicker :
        OnboardingAdapterItem(OnboardingToolbarPositionPickerViewHolder.LAYOUT_ID)

    /**
     * Spacer.
     */
    object BottomSpacer : OnboardingAdapterItem(BottomSpacerViewHolder.LAYOUT_ID)

    /**
     * Returns true if this item represents the same value as other.
     */
    open fun sameAs(other: OnboardingAdapterItem) = this::class == other::class

    /**
     * Returns a payload if there's been a change or null if not.
     */
    open fun getChangePayload(newItem: OnboardingAdapterItem): Any? = null

    /**
     * Returns true if this item represents the same value as the other.
     */
    open fun contentsSameAs(other: OnboardingAdapterItem) = this::class == other::class
}
