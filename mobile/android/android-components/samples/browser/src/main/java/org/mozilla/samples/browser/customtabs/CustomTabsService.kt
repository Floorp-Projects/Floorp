/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.customtabs

import mozilla.components.concept.engine.Engine
import mozilla.components.feature.customtabs.AbstractCustomTabsService
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.service.digitalassetlinks.RelationChecker
import org.mozilla.samples.browser.ext.components

class CustomTabsService : AbstractCustomTabsService() {
    override val engine: Engine by lazy { components.engine }
    override val customTabsServiceStore: CustomTabsServiceStore by lazy { components.customTabsStore }
    override val relationChecker: RelationChecker by lazy { components.relationChecker }
}
