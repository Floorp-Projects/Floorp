/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.components.features.FirefoxAccountsAuthFeature
import org.mozilla.fenix.test.Mockable

/**
 * Component group which encapsulates foreground-friendly services.
 */
@Mockable
class Services(
    private val accountManager: FxaAccountManager
) {
    val accountsAuthFeature by lazy {
        FirefoxAccountsAuthFeature(
            accountManager,
            redirectUrl = BackgroundServices.REDIRECT_URL
        )
    }
}
