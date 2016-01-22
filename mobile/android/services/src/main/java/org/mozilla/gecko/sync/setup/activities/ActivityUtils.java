/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.db.BrowserContract;

public class ActivityUtils {
  /**
   * Open a URL in Fennec, if one is provided; or just open Fennec.
   *
   * @param context Android context.
   * @param url to visit, or null to just open Fennec.
   */
  public static void openURLInFennec(final Context context, final String url) {
    Intent intent;
    if (url != null) {
      intent = new Intent(Intent.ACTION_VIEW);
      intent.setData(Uri.parse(url));
    } else {
      intent = new Intent(Intent.ACTION_MAIN);
    }
    intent.setClassName(GlobalConstants.BROWSER_INTENT_PACKAGE, GlobalConstants.BROWSER_INTENT_CLASS);
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    intent.putExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, true);
    context.startActivity(intent);
  }
}
