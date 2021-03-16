package org.mozilla.focus.customtabs

import mozilla.components.concept.engine.Engine
import mozilla.components.feature.customtabs.AbstractCustomTabsService
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import org.mozilla.focus.ext.components

class CustomTabsService : AbstractCustomTabsService() {
    override val customTabsServiceStore: CustomTabsServiceStore by lazy { components.customTabsStore }
    override val engine: Engine by lazy { components.engine }
}
