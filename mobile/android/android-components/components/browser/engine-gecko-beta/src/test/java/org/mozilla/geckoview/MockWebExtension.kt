/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

import mozilla.components.support.test.mock
import org.mozilla.gecko.util.GeckoBundle

class MockWebExtension(bundle: GeckoBundle) : WebExtension(DelegateControllerProvider { mock() }, bundle)
class MockCreateTabDetails(bundle: GeckoBundle) : WebExtension.CreateTabDetails(bundle)
class MockUpdateTabDetails(bundle: GeckoBundle) : WebExtension.UpdateTabDetails(bundle)
