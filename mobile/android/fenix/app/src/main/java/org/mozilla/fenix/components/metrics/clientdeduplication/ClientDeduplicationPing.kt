/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.clientdeduplication

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.fenix.GleanMetrics.Activation
import org.mozilla.fenix.GleanMetrics.ClientDeduplication
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.components.metrics.MetricsUtils.getHashedIdentifier

/**
 * Class to help construct and send the `clientDeduplication` ping.
 */
class ClientDeduplicationPing(private val context: Context) {
    private val customHashingSalt = "bug-1813195-02-2023"

    /**
     * Fills the metrics and triggers the 'clientDeduplication' ping.
     */
    internal fun triggerPing(active: Boolean) {
        CoroutineScope(Dispatchers.IO).launch {
            val hashedId = getHashedIdentifier(context, customHashingSalt)

            // Record the metrics.
            if (hashedId != null) {
                // We have a valid, hashed Google Advertising ID.
                Activation.identifier.set(hashedId)
                ClientDeduplication.validAdvertisingId.set(true)
            } else {
                ClientDeduplication.validAdvertisingId.set(false)
            }
            ClientDeduplication.experimentTimeframe.set(customHashingSalt)

            // Set the reason based on if the app is foregrounded or backgrounded.
            val reason = if (active) {
                Pings.clientDeduplicationReasonCodes.active
            } else {
                Pings.clientDeduplicationReasonCodes.inactive
            }

            // Submit the ping.
            Pings.clientDeduplication.submit(reason)
        }
    }
}
