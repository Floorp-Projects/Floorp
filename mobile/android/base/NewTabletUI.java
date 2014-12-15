/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;

import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.HardwareUtils;

@RobocopTarget
public class NewTabletUI {
    public static boolean isEnabled(final Context context) {
        return HardwareUtils.isTablet();
    }
}
