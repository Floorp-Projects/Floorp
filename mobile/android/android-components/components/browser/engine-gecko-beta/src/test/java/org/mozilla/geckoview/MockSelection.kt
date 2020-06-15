/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

import org.mozilla.gecko.util.GeckoBundle

class MockSelection(bundle: GeckoBundle) : GeckoSession.SelectionActionDelegate.Selection(bundle, emptySet(), null)