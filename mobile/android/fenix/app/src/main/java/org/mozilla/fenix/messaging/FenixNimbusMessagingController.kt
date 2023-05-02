/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.service.nimbus.messaging.NimbusMessagingController
import mozilla.components.service.nimbus.messaging.NimbusMessagingStorage
import org.mozilla.fenix.BuildConfig

/**
 * Bookkeeping for message actions in terms of Glean messages and the messaging store.
 * Specialized implementation of NimbusMessagingController defining the deepLinkScheme
 * used by Fenix
 */
class FenixNimbusMessagingController(
    messagingStorage: NimbusMessagingStorage,
    now: () -> Long = { System.currentTimeMillis() },
) : NimbusMessagingController(
    messagingStorage = messagingStorage,
    deepLinkScheme = BuildConfig.DEEP_LINK_SCHEME,
    now = now,
)
