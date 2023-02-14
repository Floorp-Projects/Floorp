/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import androidx.recyclerview.widget.DiffUtil
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsAdapter

/**
 * Adapter for a list of sites that are exempted from Tracking Protection,
 * along with controls to remove the exception.
 */
class TrackingProtectionExceptionsAdapter(
    interactor: TrackingProtectionExceptionsInteractor,
) : ExceptionsAdapter<TrackingProtectionException>(interactor, DiffCallback) {

    override val deleteButtonLayoutId = R.layout.delete_exceptions_button
    override val headerDescriptionResource = R.string.enhanced_tracking_protection_exceptions

    override fun wrapAdapterItem(item: TrackingProtectionException) =
        TrackingProtectionAdapterItem(item)

    data class TrackingProtectionAdapterItem(
        override val item: TrackingProtectionException,
    ) : AdapterItem.Item<TrackingProtectionException>() {
        override val url get() = item.url
    }

    internal object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            when (oldItem) {
                AdapterItem.DeleteButton, AdapterItem.Header -> oldItem === newItem
                is TrackingProtectionAdapterItem ->
                    newItem is TrackingProtectionAdapterItem && oldItem.item.url == newItem.item.url
                else -> false
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }
}
