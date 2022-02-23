/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.IGeckoEditableChild;
import org.mozilla.gecko.gfx.ISurfaceAllocator;

interface IProcessManager {
    void getEditableParent(in IGeckoEditableChild child, long contentId, long tabId);
    // Returns the interface that child processes should use to allocate Surfaces.
    ISurfaceAllocator getSurfaceAllocator();
}
