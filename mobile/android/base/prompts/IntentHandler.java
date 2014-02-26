/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import android.content.Intent;

public interface IntentHandler {
    public void onIntentSelected(Intent intent, int position);
    public void onCancelled();
}
