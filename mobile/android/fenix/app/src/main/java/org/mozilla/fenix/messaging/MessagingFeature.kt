/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction

/**
 * A message observer that updates the provided.
 */
class MessagingFeature(val appStore: AppStore) : LifecycleAwareFeature {

    override fun start() {
        appStore.dispatch(MessagingAction.Evaluate(FenixMessageSurfaceId.HOMESCREEN))
    }

    override fun stop() = Unit
}
