/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.concept.SelectablePromptView

/**
 * Delegate for address picker
 */
interface AddressDelegate {
    /**
     * The [SelectablePromptView] used for [AddressPicker] to display a
     * selectable prompt list of address options.
     */
    val addressPickerView: SelectablePromptView<Address>?

    /**
     * Callback invoked when the user clicks "Manage addresses" from
     * select address prompt.
     */
    val onManageAddresses: () -> Unit
}

/**
 * Default implementation for address picker delegate
 */
class DefaultAddressDelegate(
    override val addressPickerView: SelectablePromptView<Address>? = null,
    override val onManageAddresses: () -> Unit = {},
) : AddressDelegate
