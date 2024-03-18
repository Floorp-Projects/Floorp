/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.view

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.concept.storage.Address
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.settings.address.ext.getAddressLabel
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A list of addresses.
 *
 * @param addresses A list of [Address] to display.
 * @param onAddressClick Invoked when the user clicks on an address.
 * @param onAddAddressButtonClick Invoked when the user clicks on the "Add address" button.
 */
@Composable
fun AddressList(
    addresses: List<Address>,
    onAddressClick: (Address) -> Unit,
    onAddAddressButtonClick: () -> Unit,
) {
    LazyColumn {
        items(addresses) { address ->
            TextListItem(
                label = address.name,
                modifier = Modifier.padding(start = 56.dp),
                description = address.getAddressLabel(),
                maxDescriptionLines = 2,
                onClick = { onAddressClick(address) },
            )
        }

        item {
            IconListItem(
                label = stringResource(R.string.preferences_addresses_add_address),
                beforeIconPainter = painterResource(R.drawable.ic_new),
                onClick = onAddAddressButtonClick,
            )
        }
    }
}

@Preview
@Composable
private fun AddressListPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            AddressList(
                addresses = listOf(
                    Address(
                        guid = "1",
                        name = "Banana Apple",
                        organization = "Mozilla",
                        streetAddress = "123 Sesame Street",
                        addressLevel3 = "",
                        addressLevel2 = "",
                        addressLevel1 = "",
                        postalCode = "90210",
                        country = "US",
                        tel = "+1 519 555-5555",
                        email = "foo@bar.com",
                        timeCreated = 0L,
                        timeLastUsed = 0L,
                        timeLastModified = 0L,
                        timesUsed = 0L,
                    ),
                ),
                onAddressClick = {},
                onAddAddressButtonClick = {},
            )
        }
    }
}
