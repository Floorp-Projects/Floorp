/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import mozilla.components.concept.engine.EngineSession

internal class EngineSessionHolder {
    var engineSession: EngineSession? = null
    var engineObserver: EngineObserver? = null
}
