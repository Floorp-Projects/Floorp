/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction

/**
 * A message observer that updates the provided.
 */
class MessagingFeature(val store: AppStore) : LifecycleAwareFeature {

    override fun start() {
        if (FeatureFlags.messagingFeature) {
            store.dispatch(MessagingAction.Evaluate)
        }
    }

    override fun stop() = Unit
}
